#ifndef Magnum_ShaderTools_GlslangConverter_h
#define Magnum_ShaderTools_GlslangConverter_h
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
 * @brief Class @ref Magnum::ShaderTools::GlslangConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/ShaderTools/AbstractConverter.h>

#include "MagnumPlugins/GlslangShaderConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_GLSLANGSHADERCONVERTER_BUILD_STATIC
    #ifdef GlslangShaderConverter_EXPORTS
        #define MAGNUM_GLSLANGSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_GLSLANGSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_GLSLANGSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_GLSLANGSHADERCONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_GLSLANGSHADERCONVERTER_EXPORT
#define MAGNUM_GLSLANGSHADERCONVERTER_LOCAL
#endif

namespace Magnum { namespace ShaderTools {

/**
@brief Glslang shader converter plugin
@m_since_latest_{plugins}

@m_keywords{GlslangShaderConverter GlslToSpirvShaderConverter}
@m_keywords{GlslShaderConverter}

Uses [Glslang](https://github.com/KhronosGroup/glslang) for GLSL validation and
GLSL to SPIR-V compilation (@ref Format::Glsl, @ref Format::Spirv).

This plugin provides the `GlslShaderConverter` and `GlslToSpirvShaderConverter`
plugins.

@m_class{m-block m-danger}

@thirdparty This library makes use of [Glslang](https://github.com/KhronosGroup/glslang),
    licensed under a mixture of @m_class{m-label m-success} **BSD 3-clause**,
    @m_class{m-label m-success} **BSD 2-clause**,
    @m_class{m-label m-success} **MIT**, @m_class{m-label m-success} **Apache**,
    @m_class{m-label m-danger} **modified GPLv3** and
    @m_class{m-label m-warning} **NVidia Software** licenses
    ([license text](https://github.com/KhronosGroup/glslang/blob/master/LICENSE.txt)).
    Please consult the license before use.

@section ShaderTools-GlslangConverter-usage Usage

This plugin depends on the @ref ShaderTools and [Glslang](https://github.com/KhronosGroup/glslang)
libraries and is built if `WITH_GLSLANGSHADERCONVERTER` is enabled when
building Magnum Plugins. To use as a dynamic plugin, load
@cpp "GlslangShaderConverter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins) and
[glslang](https://github.com/KhronosGroup/glslang) repositories and do the
following. If you want to use system-installed Glslang, omit the first part and
point `CMAKE_PREFIX_PATH` to their installation dir if necessary.

@code{.cmake}
# Skip tests, external dependencies, glslangValidator executable, SPIR-V Tools
# integration and HLSL support, which Magnum doesn't use
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
# If you don't do this on version 10-11.0.0, CMake will complain that
# Glslang::Glslang depends on a non-existent path <project>/build/include in
# INTERFACE_INCLUDE_DIRECTORIES
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include")
add_subdirectory(glslang EXCLUDE_FROM_ALL)

set(WITH_GLSLANGSHADERCONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::GlslangShaderConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindGlslang.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindGlslang.cmake)
into your `modules/` directory, request the `GlslangShaderConverter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::GlslangShaderConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED GlslangShaderConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::GlslangShaderConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section ShaderTools-GlslangConverter-conversion Compiling GLSL to SPIR-V

Use one of the @ref convertDataToData(), @ref convertDataToFile(),
@ref convertFileToData() or @ref convertFileToFile() APIs to compile a GLSL
source for a particular stage to SPIR-V. Only GLSL 1.40 (OpenGL 3.2) and higher
is accepted by Glslang for compilation to SPIR-V, earlier versions can be only
validated. See @ref ShaderTools-GlslangConverter-stages and
@ref ShaderTools-GlslangConverter-format below for details on how to specify a
shader stage, input/output format version and target environment.

@section ShaderTools-GlslangConverter-validation GLSL validation

Use @ref validateData() or @ref validateFile() to validate a GLSL file. Unlike
SPIR-V compilation, all versions starting from GLSL 1.10 (OpenGL 2.0) can be
validated. Note that in some cases, such as opening an inaccessible file or an
assembly error the validation function can return @cpp {false, ""} @ce and
print a message to the error output instead.

Validation results are highly dependent on the target format and version set
using @ref setOutputFormat(), see @ref ShaderTools-GlslangConverter-format
below for details. Additional validation options can be set through the
@ref ShaderTools-GlslangConverter-configuration "plugin-specific config".

@section ShaderTools-GlslangConverter-includes Processing #include directives

If the `GL_GOOGLE_include_directive` extension is enabled (which, crazily
enough, isn't listed in any registry nor has any public specification), it's
possible to @cpp #include @ce other source files. You can also use the
extension directive to distinguish between offline compilation and runtime
compilation by a GL driver (in which case you'd add the extra sources for
example with a sequence of @ref GL::Shader::addSource() calls):

@code{.glsl}
#ifdef GL_GOOGLE_include_directive
#extension GL_GOOGLE_include_directive: require
#include "fullScreenTriangle.glsl"
#include "constants.glsl"
#endif
@endcode

Currently, only @cpp #include "file" @ce directives are implemented in the
plugin, system includes using @cpp #include <file> @ce are reserved for future
use.

@m_class{m-note m-warning}

@par
    The @gl_extension{ARB,shading_language_include} extension isn't supported
    due to subtle sematical differences --- the [offical reasoning](https://github.com/KhronosGroup/glslang/issues/249)
    is that this extension relies on shader sources being supplied at runtime
    which makes it impossible to validate the shader offline.

If you validate or convert using @ref validateFile(), @ref convertFileToFile()
or @ref convertFileToData(), this will work automatically, with includes being
searched for relative to the top-level file. If you are validating/converting
data or when need more flexibility such as custom include paths, you also
supply an @ref ShaderTools-AbstractConverter-usage-callbacks "input file callback",
which will then get called for all encountered files.

While it's possible that the callback gets called multiple times for a single
file due to common files being included from multiple places, a
@ref InputFileCallbackPolicy::LoadTemporary is guaranteed to be followed by a
matching @ref InputFileCallbackPolicy::Close before a
@ref InputFileCallbackPolicy::LoadTemporary happens again --- or, in other
words, @ref InputFileCallbackPolicy::Close is never called twice for the same
file without a corresponding @ref InputFileCallbackPolicy::LoadTemporary in
between. This means the user callbacks don't need to implement any kind of
reference counting, that's handled on the plugin side.

@section ShaderTools-GlslangConverter-stages Shader stages

When validating or converting files using @ref validateFile(),
@ref convertFileToFile() or @ref convertFileToData() and passing
@ref Stage::Unspecified, shader stage is detected based on filename extension
suffix:

-   `*.vert` for @ref Stage::Vertex
-   `*.frag` for @ref Stage::Fragment
-   `*.geom` for @ref Stage::Geometry
-   `*.tesc` for @ref Stage::TessellationControl
-   `*.tese` for @ref Stage::TessellationEvaluation
-   `*.comp` for @ref Stage::Compute
-   `*.rgen` for @ref Stage::RayGeneration
-   `*.rahit` for @ref Stage::RayAnyHit
-   `*.rchit` for @ref Stage::RayClosestHit
-   `*.rmiss` for @ref Stage::RayMiss
-   `*.rint` for @ref Stage::RayIntersection
-   `*.rcall` for @ref Stage::RayCallable
-   `*.task` for @ref Stage::MeshTask
-   `*.mesh` for @ref Stage::Mesh

Similarly is done for filenames ending with `*.<stage>.glsl`. If none of above
matches or if validating/converting data instead of a file,
@ref Stage::Unspecified is treated the same as @ref Stage::Vertex.

@section ShaderTools-GlslangConverter-format Input and output format and version

The @p format passed to @ref setInputFormat() has to be either
@ref Format::Unspecified or @ref Format::Glsl. The GLSL version is taken from
the @cpp #version @ce directive, if present in the source, and defaults to
@cpp 110 @ce (GLSL 1.10, OpenGL 2.0) if not specified. It can be forcibly
overriden with the @p version parameter to one of the following values,
equivalently to allowed @cpp #version @ce directives:

-   `110` for GLSL 1.10 (OpenGL 2.0)
-   `120` for GLSL 1.20 (OpenGL 2.1)
-   `130` for GLSL 1.30 (OpenGL 3.0)
-   `140` for GLSL 1.40 (OpenGL 3.1)
-   `150` for GLSL 1.50 compatibility profile (OpenGL 3.2)
-   `150 core` for GLSL 1.50 core profile (OpenGL 3.2)
-   `330` for GLSL 3.30 compatibility profile (OpenGL 3.3)
-   `330 core` for GLSL 3.30 core profile (OpenGL 3.3)
-   `400` for GLSL 4.00 compatibility profile (OpenGL 4.0)
-   `400 core` for GLSL 4.00 core profile (OpenGL 4.0)
-   `410` for GLSL 4.10 compatibility profile (OpenGL 4.1)
-   `410 core` for GLSL 4.10 core profile (OpenGL 4.1)
-   `420` for GLSL 4.20 compatibility profile (OpenGL 4.2)
-   `420 core` for GLSL 4.20 core profile (OpenGL 4.2)
-   `430` for GLSL 4.30 compatibility profile (OpenGL 4.3)
-   `430 core` for GLSL 4.30 core profile (OpenGL 4.3)
-   `440` for GLSL 4.40 compatibility profile (OpenGL 4.4)
-   `440 core` for GLSL 4.40 core profile (OpenGL 4.4)
-   `450` for GLSL 4.50 compatibility profile (OpenGL 4.5)
-   `450 core` for GLSL 4.50 core profile (OpenGL 4.5)
-   `460` for GLSL 4.60 compatibility profile (OpenGL 4.6)
-   `460 core` for GLSL 4.60 core profile (OpenGL 4.6)
-   `100 es` for GLSL ES 1.00 (OpenGL ES 2.0)
-   `300 es` for GLSL ES 3.00 (OpenGL ES 3.0)
-   `310 es` for GLSL ES 3.10 (OpenGL ES 3.1)
-   `320 es` for GLSL ES 3.20 (OpenGL ES 3.2)

The @p format passed to @ref setOutputFormat() has to be either
@ref Format::Unspecified or @ref Format::Spirv. The @p version is divided
between target and SPIR-V version, and by default targets Vulkan 1.0 and SPIR-V
1.0. You can override using the second parameter passed to
@ref setOutputFormat() either by specifying just the target, having the SPIR-V
version implicit:

-   `opengl` for generic OpenGL without any SPIR-V rules applied (validation
    only)
-   `opengl4.5` for OpenGL 4.5, implicitly with SPIR-V 1.0
-   `vulkan1.0` for Vulkan 1.0, implicitly with SPIR-V 1.0
-   `vulkan1.1` for Vulkan 1.1, implicitly with SPIR-V 1.3
-   `vulkan1.2` for Vulkan 1.2, implicitly with SPIR-V 1.5

Or by specifying a `<target> spv<major>.<minor>` version, where `<target>` is
one of the above and the `<major>`/`<minor>` is from the range of 1.0 to 1.5.
So for example `vulkan1.1 spv1.4` will target Vulkan 1.1 with SPIR-V 1.4
(instead of the default SPIR-V 1.3).

In case of validation and @p version set to `opengl` or `opengl4.5`, the
implicit @ref Format::Unspecified means no SPIR-V specific rules will be
enforced (like explicit uniform locations) in order to make it possible to
validate non-SPIR-V shaders such as GLSL ES or WebGL ones. In case of
`opengl4.5` you can set @ref Format::Spirv to enforce SPIR-V rules as well, and
thus behave the same as when doing a SPIR-V conversion. Using a @p version in
the `opengl4.5 spv<major>.<minor>` form will also imply @ref Format::Spirv. For
Vulkan validation and SPIR-V conversion, @ref Format::Unspecified is treated
the same way as @ref Format::Spirv.

Apart from imposing various target-specific restrictions on the GLSL source,
the `openglX.Y` target implicitly adds @cpp #define GL_SPIRV @ce (as specified
by @gl_extension{ARB,gl_spirv}), while `vulkanX.Y` adds @cpp #define VULKAN @ce
(as specified by @m_class{m-doc-external} [GL_KHR_vulkan_glsl](https://github.com/KhronosGroup/GLSL/blob/master/extensions/khr/GL_KHR_vulkan_glsl.txt)).
Either of those macros is always defined for conversion, for validation it's
defined only if SPIR-V validation is included, as defined in the paragraph
above.

@section ShaderTools-GlslangConverter-debug-info-level Debug info level

By default, the converter outputs SPIR-V without any debug information. You
can control this using @ref setDebugInfoLevel():

-   `0` or the empty default generates no debug info
-   `1` makes the input GLSL source embedded in the `OpSource` instruction
    (including the filename, if converting from a file), together with `OpLine`
    providing line info for the instructions and `OpModuleProcessed` describing
    what all processing steps were taken by Glslang

@section ShaderTools-GlslangConverter-configuration Plugin-specific config

It's possible to tune various compiler and validator options through
@ref configuration(). There's also a configurable set of builtins and limits,
affecting validation and compilation results. See below for all options and
their default values.

@snippet MagnumPlugins/GlslangShaderConverter/GlslangShaderConverter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_GLSLANGSHADERCONVERTER_EXPORT GlslangConverter: public AbstractConverter {
    public:
        /** @brief Initialize the Glslang library */
        static void initialize();

        /** @brief Finalize the Glslang library */
        static void finalize();

        /** @brief Plugin manager constructor */
        explicit GlslangConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL ConverterFeatures doFeatures() const override;
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL void doSetInputFormat(Format format, Containers::StringView version) override;
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL void doSetOutputFormat(Format format, Containers::StringView version) override;

        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL void doSetDefinitions(Containers::ArrayView<const std::pair<Containers::StringView, Containers::StringView>> definitions) override;
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL void doSetDebugInfoLevel(Containers::StringView level) override;

        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL std::pair<bool, Containers::String> doValidateFile(Stage stage, Containers::StringView filename) override;
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL std::pair<bool, Containers::String> doValidateData(Stage stage, Containers::ArrayView<const char> data) override;
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL bool doConvertFileToFile(Magnum::ShaderTools::Stage stage, Containers::StringView from, Containers::StringView to) override;
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL Containers::Array<char> doConvertFileToData(Magnum::ShaderTools::Stage stage, Containers::StringView filename) override;
        MAGNUM_GLSLANGSHADERCONVERTER_LOCAL Containers::Array<char> doConvertDataToData(Magnum::ShaderTools::Stage stage, Containers::ArrayView<const char> data) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
