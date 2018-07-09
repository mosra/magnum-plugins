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

#include "MagnumPlugins/TinyGltfImporter/configure.h"

namespace Magnum { namespace Trade {

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC
    #ifdef TinyGltfImporter_EXPORTS
        #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_TINYGLTFIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_TINYGLTFIMPORTER_EXPORT
#define MAGNUM_TINYGLTFIMPORTER_LOCAL
#endif

/**
@brief TinyGltf importer plugin

Imports glTF and binary glTF using the [TinyGLTF](https://github.com/syoyo/tinygltf)
library.

This plugin depends on the @ref Trade library and is built if
`WITH_TINYGLTFIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, you need to load the @cpp "TinyGltfImporter" @ce plugin from
`MAGNUM_PLUGINS_IMPORTER_DIR`. To use as a static plugin or as a dependency of
another plugin with CMake, you need to request the `TinyGltfImporter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::TinyGltfImporter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.

This plugin provides `GltfImporter` and `GlbImporter` plugins.

@section Trade-TinyGltfImporter-defaults Defaults

As glTF leaves the defaults of some properties to the application, the
following defaults have been chosen for this importer:

-   Sampler
    -   Minification/magnification/mipmap filter: @ref SamplerFilter::Linear,
        @ref SamplerMipmap::Linear
    -   Wrapping (all axes): @ref SamplerWrapping::Repeat
-   Material
    -   Diffuse color: @cpp 0xffffff_rgbf @ce
    -   Specular color: @cpp 0x000000_rgbf @ce
    -   Shininess: @cpp 1.0f @ce
    -   Ambient color: @cpp 0x000000_rgbf @ce

@section Trade-TinyGltfImporter-limitations Behavior and limitations

-   Importer requires no specific JSON node in glTF file (like `accessors`) so
    it can be used to import only light data, for example.

@subsection Trade-TinyGltfImporter-limitations-lights Light import

-   Light intensity is not yet supported due to glTF extension draft state.

@subsection Trade-TinyGltfImporter-limitations-meshes Mesh import

-   Only the first mesh of a multi-primitive mesh is imported.
-   Meshes with interleaved vertex data/buffer views are not supported.

@subsection Trade-TinyGltfImporter-limitations-materials Material import

-   All materials are imported as @ref Trade::PhongMaterialData with ambient
    color always set to @cpp 0x000000_rgbf @ce

@subsection Trade-TinyGltfImporter-limitations-textures Texture import

-   Texture type is always @ref Trade::TextureData::Type::Texture2D, as glTF
    doesn't support anything else
-   Z coordinate of @ref Trade::TextureData::wrapping() is always
    @ref SamplerWrapping::Repeat, as glTF doesn't support 3D textures

@section Trade-TinyGltfImporter-state Access to internal importer state

Access to the underlying TinyGLTF structures it is provided through
importer-specific data accessors:

-   @ref AbstractMaterialData::importerState() returns `tinygltf::Material`
    structure
-   @ref CameraData::importerState() returns `tinygltf::Camera` structure
-   @ref TextureData::importerState() returns `tinygltf::Texture` structure
-   @ref MeshData3D::importerState() returns `tinygltf::Mesh` structure
-   @ref ObjectData3D::importerState() returns `tinygltf::Node` structure
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
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doMesh3DForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doMesh3DName(const UnsignedInt) override;
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
