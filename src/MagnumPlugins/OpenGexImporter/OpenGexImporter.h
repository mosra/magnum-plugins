#ifndef Magnum_Trade_OpenGexImporter_h
#define Magnum_Trade_OpenGexImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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

/** @file
 * @brief Class @ref Magnum::Trade::OpenGexImporter
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/OpenGexImporter/visibility.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/OpenDdl.h"

namespace Magnum { namespace Trade {

/**
@brief OpenGEX importer

Imports the [OpenDDL](http://openddl.org)-based [OpenGEX](http://opengex.org)
format.

Supports importing of scene, object, camera, mesh, texture and image data.

This plugin depends on @ref AnyImageImporter plugin. It is built if
`WITH_OPENGEXIMPORTER` is enabled when building Magnum Plugins. To use dynamic
plugin, you need to load `OpenGexImporter` plugin from
`MAGNUM_PLUGINS_IMPORTER_DIR`. To use static plugin, you need to request
`OpenGexImporter` component of `MagnumPlugins` package in CMake and link to
`MagnumPlugins::OpenGexImporter`. See @ref building-plugins, @ref cmake-plugins
and @ref plugins for more information.

### Behavior and limitations

-   `half` data type results in parsing error.
-   On OpenGL ES, usage of double type and on WebGL additionally also usage of
    64bit integer types results in parsing error.

#### Scene hierarchy import

-   Object-only transformations are not supported.
-   Additional material references after the first one for given geometry node
    are ignored.
-   Geometry node visibility, shadow and motion blur properties are ignored.

#### Light import

-   Light attenuation properties are not yet supported.
-   Light textures are not yet supported.

#### Mesh import

-   64bit indices are not supported.
-   Quads are not supported.
-   Additional mesh LoDs after the first one are ignored.
-   `w` coordinate for vertex positions and normals is ignored if present.

#### Material import

-   Two-sided property is ignored.
-   All materials are imported as @ref Trade::PhongMaterialData with ambient
    color always set to `{0.0f, 0.0f, 0.0f}`.
-   Alpha channel of colors is ignored.
-   `emission`, `opacity` and `transparency` attributes are not supported.
-   `normal` textures are not supported.

#### Texture import

-   Texture coordinate transformation is ignored.
-   Textures using other than the first coordinate set are not supported.
-   Texture type is always @ref Trade::TextureData::Type::Texture2D, wrapping
    is always @ref Sampler::Wrapping::ClampToEdge, minification and
    magnification is @ref Sampler::Filter::Linear and mipmap selection is
    @ref Sampler::Mipmap::Linear.
-   If multiple textures have the same image filename string, given image is
    present in the image list only once. Note that only a simple string
    comparison is used without any path normalization.

### Access to internal importer state

Generic importer for OpenDDL files is implemented in @ref OpenDdl::Document
class available as part of this plugin and access to it is provided through
importer-specific data accessors:

-   Calling @ref importerState() returns pointer to parsed @ref OpenDdl::Document
-   Calling `*::importerState()` on data class instances returned from this
    importer return pointer to @ref OpenDdl::Structure of these types:
    -   @ref AbstractMaterialData::importerState() returns @ref OpenGex::Material
        structure
    -   @ref CameraData::importerState() returns @ref OpenGex::CameraObject
        structure
    -   @ref TextureData::importerState() returns @ref OpenGex::Texture
        structure
    -   @ref MeshData3D::importerState() returns @ref OpenGex::GeometryObject
        structure
    -   @ref ObjectData3D::importerState() returns @ref OpenGex::Node,
        @ref OpenGex::BoneNode, @ref OpenGex::GeometryNode,
        @ref OpenGex::CameraNode or @ref OpenGex::LightNode structure
*/
class MAGNUM_TRADE_OPENGEXIMPORTER_EXPORT OpenGexImporter: public AbstractImporter {
    public:
        /**
         * @brief Default constructor
         *
         * In case you want to open images, use
         * @ref OpenGexImporter(PluginManager::Manager<AbstractImporter>&)
         * instead.
         */
        explicit OpenGexImporter();

        /**
         * @brief Constructor
         *
         * The plugin needs access to plugin manager for importing images.
         */
        explicit OpenGexImporter(PluginManager::Manager<AbstractImporter>& manager);

        /** @brief Plugin manager constructor */
        explicit OpenGexImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~OpenGexImporter();

        /**
         * @brief Importer state
         *
         * Provides access to the parsed OpenDDL document. See class
         * documentation for more information.
         */
        const OpenDdl::Document* importerState() const {
            return static_cast<const OpenDdl::Document*>(AbstractImporter::importerState());
        }

    private:
        struct Document;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL Features doFeatures() const override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL void doClose() override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL Int doDefaultScene() override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doCameraCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::optional<CameraData> doCamera(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doObject3DCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL Int doObject3DForName(const std::string& name) override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::string doObject3DName(UnsignedInt id) override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::unique_ptr<ObjectData3D> doObject3D(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doLightCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::optional<LightData> doLight(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doMesh3DCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::optional<MeshData3D> doMesh3D(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL Int doMaterialForName(const std::string& name) override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::string doMaterialName(UnsignedInt id) override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::unique_ptr<AbstractMaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL std::optional<ImageData2D> doImage2D(UnsignedInt id) override;

        MAGNUM_TRADE_OPENGEXIMPORTER_LOCAL const void* doImporterState() const override;

        std::unique_ptr<Document> _d;
};

}}

#endif
