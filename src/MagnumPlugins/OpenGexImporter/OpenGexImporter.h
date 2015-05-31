#ifndef Magnum_Trade_OpenGexImporter_h
#define Magnum_Trade_OpenGexImporter_h
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

/** @file
 * @brief Class @ref Magnum::Trade::OpenGexImporter
 */

#include <Magnum/Trade/AbstractImporter.h>

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
`${MAGNUMPLUGINS_OPENGEXIMPORTER_LIBRARIES}`. To use this as a dependency of
another plugin, you additionally need to add
`${MAGNUMPLUGINS_OPENGEXIMPORTER_INCLUDE_DIRS}` to include path. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

### Behavior and limitations

-   `half` data type results in parsing error.
-   On OpenGL ES, usage of double type and on WebGL additionally also usage of
    64bit integer types results in parsing error.

#### Scene hierarchy import

-   Object-only transformations are not supported.
-   Additional material references after the first one for given geometry node
    are ignored.
-   Geometry node visibility, shadow and motion blur properties are ignored.
-   Camera and light object data are not supported yet, bone node is parsed as
    @ref Trade::ObjectInstanceType3D::Empty.

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

Generic importer for OpenDDL files is implemented in @ref OpenDdl::Document
class available as part of this plugin.
*/
class OpenGexImporter: public AbstractImporter {
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
        explicit OpenGexImporter(PluginManager::AbstractManager& manager, std::string plugin);

        ~OpenGexImporter();

    private:
        struct Document;

        Features doFeatures() const override;

        bool doIsOpened() const override;
        void doOpenData(Containers::ArrayView<const char> data) override;
        void doOpenFile(const std::string& filename) override;
        void doClose() override;

        Int doDefaultScene() override;
        UnsignedInt doSceneCount() const override;
        std::optional<SceneData> doScene(UnsignedInt id) override;

        UnsignedInt doCameraCount() const override;
        std::optional<CameraData> doCamera(UnsignedInt id) override;

        UnsignedInt doObject3DCount() const override;
        Int doObject3DForName(const std::string& name) override;
        std::string doObject3DName(UnsignedInt id) override;
        std::unique_ptr<ObjectData3D> doObject3D(UnsignedInt id) override;

        UnsignedInt doMesh3DCount() const override;
        std::optional<MeshData3D> doMesh3D(UnsignedInt id) override;

        UnsignedInt doMaterialCount() const override;
        Int doMaterialForName(const std::string& name) override;
        std::string doMaterialName(UnsignedInt id) override;
        std::unique_ptr<AbstractMaterialData> doMaterial(UnsignedInt id) override;

        UnsignedInt doTextureCount() const override;
        std::optional<TextureData> doTexture(UnsignedInt id) override;

        UnsignedInt doImage2DCount() const override;
        std::optional<ImageData2D> doImage2D(UnsignedInt id) override;

        std::unique_ptr<Document> _d;
};

}}

#endif
