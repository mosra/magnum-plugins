#ifndef Magnum_ShaderTools_GlslOptimizerConverter_h
#define Magnum_ShaderTools_GlslOptimizerConverter_h
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

/** @file
 * @brief Class @ref Magnum::ShaderTools::GlslOptimizerConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/ShaderTools/AbstractConverter.h>

#include "MagnumPlugins/GlslOptimizerShaderConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_BUILD_STATIC
    #ifdef GlslOptimizerShaderConverter_EXPORTS
        #define MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_EXPORT
#define MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_LOCAL
#endif

namespace Magnum { namespace ShaderTools {

/**
@brief GLSL-optimizer shader converter plugin
@m_since_latest_{plugins}

@m_keywords{GlslOptimizerShaderConverter}
@m_keywords{GlslShaderConverter}

Uses [glsl-optimizer](https://github.com/aras-p/glsl-optimizer) for GLSL
optimization (@ref Format::Glsl).

This plugin provides the `GlslShaderConverter` plugin.

@m_class{m-block m-danger}

@thirdparty This plugin makes use of the
    [glsl-optimizer](https://github.com/aras-p/glsl-optimizer) library,
    licensed under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/aras-p/glsl-optimizer),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)). It
    requires attribution for public use.

@section ShaderTools-GlslOptimizerConverter-usage Usage

TODO TODO
*/
class MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_EXPORT GlslOptimizerConverter: public AbstractConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit GlslOptimizerConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_LOCAL ConverterFeatures doFeatures() const override;
        MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_LOCAL void doSetInputFormat(Format format, Containers::StringView version) override;
        MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_LOCAL void doSetOutputFormat(Format format, Containers::StringView version) override;

        MAGNUM_GLSLOPTIMIZERSHADERCONVERTER_LOCAL Containers::Array<char> doConvertDataToData(Magnum::ShaderTools::Stage stage, Containers::ArrayView<const char> data) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
