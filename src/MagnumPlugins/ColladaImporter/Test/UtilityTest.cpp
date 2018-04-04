/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
              Vladimír Vondruš <mosra@centrum.cz>

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
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Math/Vector3.h>

#include "MagnumPlugins/ColladaImporter/Utility.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct ColladaImporterUtilityTest: TestSuite::Tester {
    explicit ColladaImporterUtilityTest();

    void parseVector();
    void parseArray();
    void parseSource();
};

ColladaImporterUtilityTest::ColladaImporterUtilityTest() {
    addTests({&ColladaImporterUtilityTest::parseVector,
              &ColladaImporterUtilityTest::parseArray,
              &ColladaImporterUtilityTest::parseSource});
}

void ColladaImporterUtilityTest::parseVector() {
    /* Extremely spaceless */
    CORRADE_COMPARE((Implementation::Utility::parseVector<Math::Vector<1, Float>>("3.14")[0]), 3.14f);

    /* Spaces */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>("     2.17  3.28  "), Vector2(2.17f, 3.28f));

    /* Nothing */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>({}), Vector2());
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>("    "), Vector2());

    /* Different size */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector3>("2.17 3.28"), Vector3(2.17f, 3.28f, 0.0f));
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector3>("2.17 3.28 5.15", 2), Vector3(2.17f, 3.28f, 0.0f));

    /* Moving from value */
    int from;
    Implementation::Utility::parseVector<Vector2>("2.17 3.28", &(from = 0));
    CORRADE_COMPARE(from, 9);

    /* Different start */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>("2.17 3.28", &(from = 5)), Vector2(3.28f, 0.0f));
    CORRADE_COMPARE(from, 9);
}

void ColladaImporterUtilityTest::parseArray() {
    /* Extremely spaceless */
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("3.14", 1), (std::vector<Float>{3.14f}));

    /* Spaces */
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("     2.17  3.28  ", 2), (std::vector<Float>{2.17f, 3.28f}));

    /* Different size */
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("2.17 3.28", 3), (std::vector<Float>{2.17f, 3.28f, 0.0f}));
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("2.17 3.28 5.15", 2), (std::vector<Float>{2.17f, 3.28f}));
}

void ColladaImporterUtilityTest::parseSource() {
    int zero = 0;
    QCoreApplication app{zero, nullptr};
    const QString namespaceDeclaration =
        "declare default element namespace \"http://www.collada.org/2005/11/COLLADASchema\";\n";
    QXmlQuery query;

    /* Open the file and load it into XQuery */
    QFile file(QString::fromStdString(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "parseSource.dae")));
    CORRADE_VERIFY(file.open(QIODevice::ReadOnly));
    CORRADE_VERIFY(query.setFocus(&file));

    std::stringstream debug;
    Error redirectError{&debug};
    CORRADE_VERIFY(Implementation::Utility::parseSource<Vector3>(query, namespaceDeclaration, "WrongTotalCount").empty());
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::mesh3D(): wrong total count in source \"WrongTotalCount\"\n");

    {
        CORRADE_EXPECT_FAIL("Swapped coordinates in source are not implemented.");
        CORRADE_COMPARE(Implementation::Utility::parseSource<Vector3>(query, namespaceDeclaration, "SwappedCoords"), (std::vector<Vector3>{Vector3(0, 1, 2)}));
    }

    CORRADE_COMPARE(Implementation::Utility::parseSource<Vector3>(query, namespaceDeclaration, "MoreElements"), (std::vector<Vector3>{
        {0, 1, 2},
        {3, 4, 5}
    }));
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::ColladaImporterUtilityTest)
