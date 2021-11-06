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

#include <Corrade/TestSuite/Tester.h>
#include <Magnum/ShaderTools/AbstractConverter.h>

#include "configure.h"

namespace Magnum { namespace ShaderTools { namespace Test { namespace {

struct GlslOptimizerConverterTest: TestSuite::Tester {
    explicit GlslOptimizerConverterTest();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractConverter> _converterManager{"nonexistent"};
};

GlslOptimizerConverterTest::GlslOptimizerConverterTest() {

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef GLSLOPTIMIZERSHADERCONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(GLSLOPTIMIZERSHADERCONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

}}}}

CORRADE_TEST_MAIN(Magnum::ShaderTools::Test::GlslOptimizerConverterTest)
