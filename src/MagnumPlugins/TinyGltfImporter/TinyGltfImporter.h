#ifndef Magnum_Trade_TinyGltfImporter_h
#define Magnum_Trade_TinyGltfImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>

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
 * @brief Class @ref Magnum::Trade::TinyGltfImporter
 */
#include <Magnum/Trade/AbstractImporter.h>
#include "MagnumPlugins/TinyGltfImporter/visibility.h"

namespace Magnum { namespace Trade {

/**
@brief TinyGltf importer plugin

This plugin depends on the @ref Trade library and is built if
`WITH_TINYGLTFIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, you need to load the @cpp "TinyGltfImporter" @ce plugin from
`MAGNUM_PLUGINS_IMPORTER_DIR`. To use as a static plugin or as a dependency of
another plugin with CMake, you need to request the `TinyGltfImporter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::TinyGltfImporter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.

@section Trade-TinyGltfImporter-limitations Behavior and limitations

-   Importer requires no specific json-node at .gltf-File like `accessors` 
so it can be used to import only lights

@subsection Trade-TinyGltfImporter-limitations-scenes Scene hierarchy import

-   Object-only transformations are not supported.
-   Additional material references after the first one for given geometry node
are ignored.
-   Geometry node visibility, shadow and motion blur properties are ignored.

@subsection Trade-TinyGltfImporter-limitations-lights Light import

-   Light attenuation properties are not yet supported.
-   Light textures are not yet supported.
-   Light intensity is not yet supported (due to gltf extension draft state)

@subsection Trade-TinyGltfImporter-limitations-meshes Mesh import

-   64bit indices are not supported.
-   Quads are not supported.
-   Additional mesh LoDs after the first one are ignored.
-   `w` coordinate for vertex positions and normals is ignored if present.

@subsection Trade-TinyGltfImporter-limitations-materials Material import

-   Two-sided property is ignored.
-   All materials are imported as @ref Trade::PhongMaterialData with ambient
color always set to `{0.0f, 0.0f, 0.0f}`.
-   Alpha channel of colors is ignored.
-   `emission`, `opacity` and `transparency` attributes are not supported.
-   `normal` textures are not supported.

@subsection Trade-TinyGltfImporter-limitations-textures Texture import

-   Texture coordinate transformation is ignored.
-   Textures using other than the first coordinate set are not supported.
-   Texture type is always @ref Trade::TextureData::Type::Texture2D

@subsection Trade-TinyGltfImporter-state Access to internal importer state

Generic importer for TinyGltf files is implemented with the use of
the tinygltf-Library and access to it is provided through importer-specific data accessors:

-   @ref AbstractMaterialData::importerState() returns @ref tinygltf::Material
structure
-   @ref CameraData::importerState() returns @ref tinygltf::Camera
structure
-   @ref TextureData::importerState() returns @ref tinygltf::Texture
structure
-   @ref MeshData3D::importerState() returns @ref tinygltf::Mesh
structure
-   @ref ObjectData3D::importerState() returns @ref tinygltf::Node
*/
class MAGNUM_TINYGLTFIMPORTER_EXPORT TinyGltfImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit TinyGltfImporter();

        /** @brief Plugin manager constructor */
        explicit TinyGltfImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~TinyGltfImporter();

    private:
        struct Document;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Features doFeatures() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL bool doIsOpened() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doCameraCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doLightCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doDefaultScene() override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doSceneCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doObject3DCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doObject3DName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doObject3DForName(const std::string& name) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL std::unique_ptr<ObjectData3D> doObject3D(UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doMesh3DCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<MeshData3D> doMesh3D(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doMaterialForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doMaterialName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::unique_ptr<AbstractMaterialData> doMaterial(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL void doClose() override;

        std::unique_ptr<Document> _d;
};

}}

#endif
