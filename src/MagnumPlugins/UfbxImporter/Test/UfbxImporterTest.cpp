/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Samuli Raivio <bqqbarbhg@gmail.com>

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
#include <string>
#include <unordered_map>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>
#include <Magnum/Math/Angle.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/PbrSpecularGlossinessMaterialData.h>
#include <Magnum/Trade/PbrClearCoatMaterialData.h>

#include "../UfbxMaterials.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

using namespace Math::Literals;
using namespace Containers::Literals;

struct UfbxImporterTest: TestSuite::Tester {
    explicit UfbxImporterTest();

    void openFile();
    void openData();
    void openFileFailed();
    void openDataFailed();
    void mesh();
    void light();
    void materialMapping();
    void blenderMaterials();
    void meshMaterials();
    void imageEmbedded();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

UfbxImporterTest::UfbxImporterTest() {
    addTests({&UfbxImporterTest::openFile,
              &UfbxImporterTest::openData,
              &UfbxImporterTest::openFileFailed,
              &UfbxImporterTest::openDataFailed});

    addTests({&UfbxImporterTest::mesh,
              &UfbxImporterTest::light});

    addTests({&UfbxImporterTest::materialMapping});

    addTests({&UfbxImporterTest::blenderMaterials,
              &UfbxImporterTest::meshMaterials,
              &UfbxImporterTest::imageEmbedded});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. */
    #ifdef UFBXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(UFBXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _manager.setPluginDirectory({});
    #endif
}

void UfbxImporterTest::openFile() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->lightCount(), 1);
    CORRADE_COMPARE(importer->cameraCount(), 1);
    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void UfbxImporterTest::openData() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx"));
    CORRADE_VERIFY(importer->openData(*data));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->lightCount(), 1);
    CORRADE_COMPARE(importer->cameraCount(), 1);
    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void UfbxImporterTest::openFileFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile("i-do-not-exist.foo"));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openData(): loading failed: File not found: i-do-not-exist.foo\n");
}

void UfbxImporterTest::openDataFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    std::ostringstream out;
    Error redirectError{&out};

    constexpr const char data[] = "what";
    CORRADE_VERIFY(!importer->openData({data, sizeof(data)}));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openData(): loading failed: Unrecognized file format\n");
}

void UfbxImporterTest::mesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "triangle.fbx")));

    CORRADE_COMPARE(importer->meshCount(), 1);

    /* FBX files don't have reliable mesh names, so go by index */
    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);

    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 6);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Tangent), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Tangent),
        Containers::arrayView<Vector3>({
            {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Bitangent), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Bitangent),
        Containers::arrayView<Vector3>({
            {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Color), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4>(MeshAttribute::Color),
        Containers::arrayView<Vector4>({
            {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 7);

    const SceneField sceneFieldVisibility = importer->sceneFieldForName("Visibility"_s);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
    CORRADE_VERIFY(scene->hasField(sceneFieldVisibility));

    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        0,
    }), TestSuite::Compare::Container);
}

void UfbxImporterTest::light() {
    std::ostringstream out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "lights.fbx")));

    CORRADE_COMPARE(importer->objectCount(), 6);
    CORRADE_COMPARE(importer->lightCount(), 6);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    const SceneField sceneFieldVisibility = importer->sceneFieldForName("Visibility"_s);
    CORRADE_VERIFY(scene->hasField(sceneFieldVisibility));

    Containers::StridedArrayView1D<const Vector3d> translations = scene->field<Vector3d>(SceneField::Translation);
    CORRADE_VERIFY(scene->fieldFlags(SceneField::Translation) & SceneFieldFlag::ImplicitMapping);

    Containers::StridedArrayView1D<const UnsignedByte> visibilities = scene->field<UnsignedByte>(sceneFieldVisibility);
    CORRADE_VERIFY(scene->fieldFlags(sceneFieldVisibility) & SceneFieldFlag::ImplicitMapping);

    /* FBX files often don't have names for "attributes" such as lights so
       we need to go through objects. In theory we could propagate names from
       the objects to attributes but this would break with instancing. */

    {
        /* The plug-in does not support the following light types:
           Ambient light(|ambientLight1)  will be exported as an inactive
           point light with the Visibility value set to off. */
        Long objectId = importer->objectForName("ambientLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "ambientLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{0.0f,0.0f,0.0f}));
        CORRADE_COMPARE(visibilities[objectId], false);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.11f, 0.12f, 0.13f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f,0.0f,0.0f}));
    }

    {
        Long objectId = importer->objectForName("directionalLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "directionalLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{1.0f,0.0f,0.0f}));
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        CORRADE_COMPARE(light->color(), (Color3{0.21f, 0.22f, 0.23f}));
        CORRADE_COMPARE(light->intensity(), 2.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f,0.0f,0.0f}));
    }

    {
        Long objectId = importer->objectForName("pointLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "pointLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{2.0f,0.0f,0.0f}));
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.31f, 0.32f, 0.33f}));
        CORRADE_COMPARE(light->intensity(), 3.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.0f,1.0f,0.0f}));
    }

    {
        Long objectId = importer->objectForName("spotLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{3.0f,0.0f,0.0f}));
        CORRADE_COMPARE(importer->objectName(objectId), "spotLight1");
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.41f, 0.42f, 0.43f}));
        CORRADE_COMPARE(light->intensity(), 4.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.0f,0.0f,1.0f}));
        CORRADE_COMPARE(light->innerConeAngle(), 12.0_degf);
        CORRADE_COMPARE(light->outerConeAngle(), 80.0_degf);
    }

    {
        Long objectId = importer->objectForName("areaLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{4.0f,0.0f,0.0f}));
        CORRADE_COMPARE(importer->objectName(objectId), "areaLight1");
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        /* Area lights are not supported */
        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(!light);
    }

    {
        Long objectId = importer->objectForName("volumeLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{5.0f,0.0f,0.0f}));
        CORRADE_COMPARE(importer->objectName(objectId), "volumeLight1");
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        /* Volume lights are not supported */
        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(!light);
    }

    CORRADE_COMPARE(out.str(),
        "Trade::UfbxImporter::light(): light type 3 is not supported\n"
        "Trade::UfbxImporter::light(): light type 4 is not supported\n");
}

void UfbxImporterTest::materialMapping() {
    /* Make sure all the ufbx maps are used at least once and that there are
       no duplicate Magnum attributes without MaterialExclusionGroup */

    Containers::ArrayView<const MaterialMapping> mappingLists[] = {
        Containers::arrayView(materialMappingFbx),
        Containers::arrayView(materialMappingPbr),
    };

    Containers::StaticArray<UfbxMaterialLayerCount, std::unordered_map<std::string, MaterialExclusionGroup>> usedAttributeNames;

    Containers::BitArray usedUfbxMaps[2] = {
        Containers::BitArray{ ValueInit, UFBX_MATERIAL_FBX_MAP_COUNT },
        Containers::BitArray{ ValueInit, UFBX_MATERIAL_PBR_MAP_COUNT },
    };

    for(UnsignedInt type = 0; type < 2; ++type) {
        for (const MaterialMapping &mapping : mappingLists[type]) {
            std::size_t layer = std::size_t(mapping.layer);

            if (mapping.valueMap >= 0)
                usedUfbxMaps[type].set(std::size_t(mapping.valueMap));
            if (mapping.factorMap >= 0)
                usedUfbxMaps[type].set(std::size_t(mapping.factorMap));

            /* Copy to std::string so we don't do unnecessary conversions on
               lookups, also this is far from performance critical */
            std::string attribute = mapping.attribute;
            std::string textureAttribute = mapping.textureAttribute;

            if (!attribute.empty()) {
                CORRADE_ITERATION(attribute);

                auto found = usedAttributeNames[layer].find(attribute);
                if (found != usedAttributeNames[layer].end()) {
                    /* If we have a duplicate material attribute name it must
                       be defined under the same exclusion group */
                    CORRADE_VERIFY(mapping.exclusionGroup != MaterialExclusionGroup{});
                    CORRADE_COMPARE(UnsignedInt(mapping.exclusionGroup), UnsignedInt(found->second));
                } else {
                    usedAttributeNames[layer].insert({ attribute, mapping.exclusionGroup });
                }

                if (textureAttribute.empty())
                    textureAttribute = attribute + "Texture";
            }

            if (!textureAttribute.empty()) {
                CORRADE_ITERATION(textureAttribute);

                auto found = usedAttributeNames[layer].find(textureAttribute);
                if (found != usedAttributeNames[layer].end()) {
                    /* If we have a duplicate material attribute name it must
                       be defined under the same exclusion group */
                    CORRADE_VERIFY(mapping.exclusionGroup != MaterialExclusionGroup{});
                    CORRADE_COMPARE(UnsignedInt(mapping.exclusionGroup), UnsignedInt(found->second));
                } else {
                    usedAttributeNames[layer].insert({ textureAttribute, mapping.exclusionGroup });
                }
            }
        }
    }

    /* Make sure all the ufbx maps are accounted for */
    /* @todo: Could we fail with an index in the message here? */
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_FBX_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        CORRADE_VERIFY(usedUfbxMaps[0][i]);
    }
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_PBR_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        CORRADE_VERIFY(usedUfbxMaps[1][i]);
    }
}

void UfbxImporterTest::blenderMaterials() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-materials.fbx")));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 3);

    {
        Containers::Optional<MaterialData> material = importer->material("Default");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);

        {
            const PhongMaterialData& phong = material->as<PhongMaterialData>();
            CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.8f, 0.8f, 0.8f, 1.0f}));
            CORRADE_COMPARE(phong.shininess(), 25.0f);
        }

        {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();
            CORRADE_COMPARE(pbr.baseColor(), (Color4{0.8f, 0.8f, 0.8f, 1.0f}));
            CORRADE_COMPARE(pbr.emissiveColor(), (Color3{0.0f, 0.0f, 0.0f}));
            CORRADE_COMPARE(pbr.metalness(), 0.0f);
            CORRADE_COMPARE(pbr.roughness(), 0.5f);
        }
    }

    {
        Containers::Optional<MaterialData> material = importer->material("Top");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);

        {
            const PhongMaterialData& phong = material->as<PhongMaterialData>();
            CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.1f, 0.2f, 0.3f, 1.0f}));
            CORRADE_COMPARE(phong.shininess(), 4.0f);
        }

        {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();
            CORRADE_COMPARE(pbr.baseColor(), (Color4{0.1f, 0.2f, 0.3f, 0.9f}));
            CORRADE_COMPARE(pbr.emissiveColor(), (Color3{0.4f, 0.5f, 0.6f}));
            CORRADE_COMPARE(pbr.metalness(), 0.7f);
            CORRADE_COMPARE(pbr.roughness(), 0.8f);
        }
    }

    {
        Containers::Optional<MaterialData> material = importer->material("Textures");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);

        {
            const PhongMaterialData& phong = material->as<PhongMaterialData>();

            {
                CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
                Containers::Optional<TextureData> texture = importer->texture(phong.diffuseTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_diffuse.png");
            }
        }

        {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.baseColorTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_diffuse.png");
            }

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::EmissiveTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.emissiveTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_emissive.png");
            }

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::RoughnessTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.roughnessTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_roughness.png");
            }

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::MetalnessTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.metalnessTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_metallic.png");
            }
        }

        {
            CORRADE_VERIFY(material->hasAttribute("opacityTexture"));
            UnsignedInt textureId = material->attribute<UnsignedInt>("opacityTexture");
            Containers::Optional<TextureData> texture = importer->texture(textureId);
            CORRADE_VERIFY(texture);
            CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_transparency.png");
        }
    }
}

void UfbxImporterTest::meshMaterials() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-materials.fbx")));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 3);
    CORRADE_COMPARE(importer->meshCount(), 3);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 0, 0,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 1, 2,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        0, 1, 2,
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE(importer->materialName(0), "Default");
    CORRADE_COMPARE(importer->materialName(1), "Top");
    CORRADE_COMPARE(importer->materialName(2), "Textures");

    /* 'Default' material is on the sides */
    {
        Containers::Optional<MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 4*2*3);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for (UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(normal[i].z(), 0.0f);
        }
    }

    /* 'Top' material is on the top face (+Z in Blender local) */
    {
        Containers::Optional<MeshData> mesh = importer->mesh(1);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 2*3);

        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> position = mesh->attribute<Vector3>(MeshAttribute::Position);
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for (UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(position[i].z(), 1.0f);
            CORRADE_COMPARE(normal[i], (Vector3{0.0f,0.0f,1.0f}));
        }
    }

    /* 'Textures' material is on the bottom face (-Z in Blender local) */
    {
        Containers::Optional<MeshData> mesh = importer->mesh(2);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 2*3);

        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> position = mesh->attribute<Vector3>(MeshAttribute::Position);
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for (UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(position[i].z(), -1.0f);
            CORRADE_COMPARE(normal[i], (Vector3{0.0f,0.0f,-1.0f}));
        }
    }

}

void UfbxImporterTest::imageEmbedded() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-materials.fbx")));
    CORRADE_VERIFY(importer->isOpened());

    UnsignedInt imageCount = importer->image2DCount();
    CORRADE_COMPARE(imageCount, 5);

    struct TestImage {
        Containers::StringView name;
        Color4ub topLeftPixelColor;
        Color4ub bottomLeftPixelColor;
    };

    TestImage testImages[] = {
        { "blender-materials.fbm\\checkerboard_diffuse.png",      0x7c5151ff_rgba, 0x3d2828ff_rgba },
        { "blender-materials.fbm\\checkerboard_emissive.png",     0x44743bff_rgba, 0x223a1dff_rgba },
        { "blender-materials.fbm\\checkerboard_roughness.png",    0x5f3228ff_rgba, 0x2f1914ff_rgba },
        { "blender-materials.fbm\\checkerboard_metallic.png",     0x63422cff_rgba, 0x312116ff_rgba },
        { "blender-materials.fbm\\checkerboard_transparency.png", 0x407177ff_rgba, 0x1f373aff_rgba },
    };

    for (const TestImage& testImage : testImages) {
        CORRADE_ITERATION(testImage.name);

        Int optionalId = importer->image2DForName(testImage.name);
        CORRADE_VERIFY(optionalId >= 0);
        UnsignedInt id = UnsignedInt(optionalId);

        CORRADE_COMPARE(importer->image2DLevelCount(id), 1);
        Containers::Optional<ImageData2D> image = importer->image2D(id);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{128,128}));
        CORRADE_COMPARE(image->pixels<Color4ub>()[0][0], testImage.bottomLeftPixelColor);
        CORRADE_COMPARE(image->pixels<Color4ub>()[127][0], testImage.topLeftPixelColor);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::UfbxImporterTest)
