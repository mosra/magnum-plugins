/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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
#include <Corrade/Containers/Array.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>

#include "Magnum/Mesh.h"
#include "Magnum/Math/Vector3.h"
#include "Magnum/Trade/ImageData.h"
#include "Magnum/Trade/MeshData3D.h"
#include "Magnum/Trade/PhongMaterialData.h"
#include "Magnum/Trade/TextureData.h"
#include "MagnumPlugins/OpenGexImporter/OpenGexImporter.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct OpenGexImporterTest: public TestSuite::Tester {
    explicit OpenGexImporterTest();

    void open();
    void openParseError();
    void openValidationError();
    void openInvalidMetric();

    void mesh();
    void meshIndexed();
    void meshEnlargeShrink();
    void meshMetrics();

    void meshInvalidPrimitive();
    void meshUnsupportedSize();
    void meshNoPositions();
    void meshMismatchedSizes();
    void meshInvalidIndexArraySubArraySize();
    void meshUnsupportedIndexType();

    void materialDefaults();
    void materialColors();
    void materialTextured();
    void materialInvalidColor();

    void texture();
    void textureInvalidCoordinateSet();

    void image();
    void imageInvalid();
};

OpenGexImporterTest::OpenGexImporterTest() {
    addTests({&OpenGexImporterTest::open,
              &OpenGexImporterTest::openParseError,
              &OpenGexImporterTest::openValidationError,
              &OpenGexImporterTest::openInvalidMetric,

              &OpenGexImporterTest::mesh,
              &OpenGexImporterTest::meshIndexed,
              &OpenGexImporterTest::meshEnlargeShrink,
              &OpenGexImporterTest::meshMetrics,

              &OpenGexImporterTest::meshInvalidPrimitive,
              &OpenGexImporterTest::meshUnsupportedSize,
              &OpenGexImporterTest::meshNoPositions,
              &OpenGexImporterTest::meshMismatchedSizes,
              &OpenGexImporterTest::meshInvalidIndexArraySubArraySize,
              &OpenGexImporterTest::meshUnsupportedIndexType,

              &OpenGexImporterTest::materialDefaults,
              &OpenGexImporterTest::materialColors,
              &OpenGexImporterTest::materialTextured,
              &OpenGexImporterTest::materialInvalidColor,

              &OpenGexImporterTest::image,
              &OpenGexImporterTest::imageInvalid,

              &OpenGexImporterTest::texture,
              &OpenGexImporterTest::textureInvalidCoordinateSet});
}

void OpenGexImporterTest::open() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openData(R"oddl(
Metric (key = "distance") { float { 0.5 } }
Metric (key = "angle") { float { 1.0 } }
Metric (key = "time") { float { 1000 } }
Metric (key = "up") { string { "z" } }
    )oddl"));
}

void OpenGexImporterTest::openParseError() {
    OpenGexImporter importer;

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.openData(R"oddl(
<collada>THIS IS COLLADA XML</collada>
    )oddl"));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid identifier on line 2\n");
}

void OpenGexImporterTest::openValidationError() {
    OpenGexImporter importer;

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.openData(R"oddl(
Metric (key = "distance") { int32 { 1 } }
    )oddl"));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected sub-structure of type OpenDdl::Type::Int in structure Metric\n");
}

void OpenGexImporterTest::openInvalidMetric() {
    OpenGexImporter importer;

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.openData(R"oddl(
Metric (key = "distance") { string { "0.5" } }
    )oddl"));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::openData(): invalid value for distance metric\n");
}

void OpenGexImporterTest::mesh() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::TriangleStrip);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 2);
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5f, 0.5f}, {0.5f, 1.0f}, {1.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2D(1), (std::vector<Vector2>{
        {0.5f, 1.0f}, {1.0f, 0.5f}, {0.5f, 0.5f}
    }));
}

void OpenGexImporterTest::meshIndexed() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(1);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}, {5.0f, 7.0f, 0.5f}
    }));
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{
        2, 0, 1, 1, 2, 3
    }));
}

void OpenGexImporterTest::meshEnlargeShrink() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(2);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 1);
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}
    }));
}

void OpenGexImporterTest::meshMetrics() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-metrics.ogex")));
    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {100.0f, -200.0f, -50.0f} /* swapped for Y up, multiplied */
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
        {1.0, -1.0, -2.5} /* swapped for Y up */
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 1);
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {1.0, 0.5} /* no change */
    }));
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{
        2 /* no change */
    }));
}

void OpenGexImporterTest::meshInvalidPrimitive() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 6);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): unsupported primitive quads\n");
}

void OpenGexImporterTest::meshUnsupportedSize() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 6);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.mesh3D(1));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): unsupported vertex array vector size 5\n");
}

void OpenGexImporterTest::meshNoPositions() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 6);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.mesh3D(2));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): no vertex position array found\n");
}

void OpenGexImporterTest::meshMismatchedSizes() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 6);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.mesh3D(3));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): mismatched vertex array sizes\n");
}

void OpenGexImporterTest::meshInvalidIndexArraySubArraySize() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 6);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.mesh3D(4));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): invalid index array subarray size 3 for MeshPrimitive::Lines\n");
}

void OpenGexImporterTest::meshUnsupportedIndexType() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 6);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.mesh3D(5));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): unsupported 64bit indices\n");
}

void OpenGexImporterTest::materialDefaults() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));

    std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);
    CORRADE_COMPARE(importer.materialName(0), "");

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_COMPARE(phong.ambientColor(), Vector3{0.0f});
    CORRADE_COMPARE(phong.diffuseColor(), Vector3{1.0f});
    CORRADE_COMPARE(phong.specularColor(), Vector3{0.0f});
    CORRADE_COMPARE(phong.shininess(), 1.0f);
}

void OpenGexImporterTest::materialColors() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 4);

    std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(1);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);
    CORRADE_COMPARE(importer.materialName(1), "colors");
    CORRADE_COMPARE(importer.materialForName("colors"), 1);

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_VERIFY(!phong.flags());
    CORRADE_COMPARE(phong.diffuseColor(), (Vector3{0.0f, 0.8f, 0.5f}));
    CORRADE_COMPARE(phong.specularColor(), (Vector3{0.5f, 0.2f, 1.0f}));
    CORRADE_COMPARE(phong.shininess(), 80.0f);
}

void OpenGexImporterTest::materialTextured() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 4);
    CORRADE_COMPARE(importer.textureCount(), 4);

    {
        std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(2);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(importer.materialName(2), "diffuse_texture");

        auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() == Trade::PhongMaterialData::Flag::DiffuseTexture);
        CORRADE_COMPARE(phong.diffuseTexture(), 1);
    } {
        std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(3);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(importer.materialName(3), "both_textures");

        auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() == (Trade::PhongMaterialData::Flag::DiffuseTexture|Trade::PhongMaterialData::Flag::SpecularTexture));
        CORRADE_COMPARE(phong.diffuseTexture(), 2);
        CORRADE_COMPARE(phong.specularTexture(), 3);
    }
}

void OpenGexImporterTest::materialInvalidColor() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material-invalid.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 1);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.material(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::material(): invalid color structure\n");
}

void OpenGexImporterTest::texture() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer.textureCount(), 2);

    std::optional<Trade::TextureData> texture = importer.texture(1);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->minificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->wrapping(), Sampler::Wrapping::ClampToEdge);
    CORRADE_COMPARE(texture->image(), 1);
}

void OpenGexImporterTest::textureInvalidCoordinateSet() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-invalid.ogex")));
    CORRADE_COMPARE(importer.textureCount(), 2);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.texture(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::texture(): unsupported texture coordinate set\n");
}

void OpenGexImporterTest::image() {
    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    OpenGexImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer.image2DCount(), 2);

    /* Check only size, as it is good enough proof that it is working */
    std::optional<Trade::ImageData2D> image = importer.image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
}

void OpenGexImporterTest::imageInvalid() {
    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    OpenGexImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-invalid.ogex")));
    CORRADE_COMPARE(importer.image2DCount(), 2);

    std::ostringstream out;
    Error::setOutput(&out);
    CORRADE_VERIFY(!importer.image2D(1));
    CORRADE_COMPARE(out.str(), "Trade::TgaImporter::openFile(): cannot open file /nonexistent.tga\n");
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::OpenGexImporterTest)
