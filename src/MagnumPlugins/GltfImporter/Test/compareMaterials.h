#ifndef Magnum_Trade_Test_compareMaterials_h
#define Magnum_Trade_Test_compareMaterials_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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
#include <Magnum/Math/Matrix.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Trade/MaterialData.h>

namespace Magnum { namespace Trade { namespace Test { namespace {

/* Used by GltfImporterTest and GltfSceneConverterTest. Should eventually get
   tested, documented and exposed as a utility in DebugTools */

void compareMaterials(const MaterialData& actual, const MaterialData& expected) {
    CORRADE_COMPARE(actual.types(), expected.types());
    CORRADE_COMPARE(actual.layerCount(), expected.layerCount());

    for(UnsignedInt layer = 0; layer != expected.layerCount(); ++layer) {
        CORRADE_ITERATION(expected.layerName(layer));
        CORRADE_COMPARE(actual.layerName(layer), expected.layerName(layer));
        CORRADE_COMPARE(actual.attributeCount(layer), expected.attributeCount(layer));
        for(UnsignedInt i = 0; i != expected.attributeCount(layer); ++i) {
            const Containers::StringView name = expected.attributeName(layer, i);
            CORRADE_ITERATION(name);
            CORRADE_VERIFY(actual.hasAttribute(layer, name));
            const MaterialAttributeType type = expected.attributeType(layer, name);
            CORRADE_COMPARE(actual.attributeType(layer, name), type);
            switch(type) {
                #define _v(type, valueType) case MaterialAttributeType::type: \
                    CORRADE_COMPARE(actual.attribute<valueType>(layer, name), expected.attribute<valueType>(layer, name)); \
                    break;
                #define _c(type) _v(type, type)
                _c(UnsignedInt)
                _c(Float)
                _c(Vector2)
                _c(Vector3)
                _c(Vector4)
                _c(Matrix3x3)
                _v(Bool, bool)
                _v(String, Containers::StringView)
                _v(TextureSwizzle, MaterialTextureSwizzle)
                #undef _c
                #undef _v
                default:
                    CORRADE_FAIL_IF(true, "Unexpected attribute type" << type);
            }
        }
    }
}

}}}}

#endif
