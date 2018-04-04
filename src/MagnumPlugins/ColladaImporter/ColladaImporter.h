#ifndef Magnum_Trade_ColladaImporter_h
#define Magnum_Trade_ColladaImporter_h
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

/** @file
 * @brief Class @ref Magnum::Trade::ColladaImporter
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/ColladaImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_COLLADAIMPORTER_BUILD_STATIC
    #ifdef ColladaImporter_EXPORTS
        #define MAGNUM_COLLADAIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_COLLADAIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_COLLADAIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_COLLADAIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_COLLADAIMPORTER_EXPORT
#define MAGNUM_COLLADAIMPORTER_LOCAL
#endif

class QCoreApplication;
class QString;

namespace Magnum { namespace Trade {

class ColladaMeshData;

/**
@brief Collada importer plugin

Imports the XML-based [COLLADA](https://collada.org/) format. Supports triangle
and quad meshes, images (delegated to @ref AnyImageImporter), Phong material
data, texture properties and scene hierarchy.

This plugin depends on the @ref Trade, @ref MeshTools and [Qt 4](https://qt.io)
libraries and the @ref AnyImageImporter plugin. It is built if
`WITH_COLLADAIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, you need to load the @cpp "ColladaImporter" @ce plugin from
`MAGNUM_PLUGINS_IMPORTER_DIR`. To use as a static plugin or as a dependency of
another plugin with CMake, you need to request the `ColladaImporter` component
of the `MagnumPlugins` package in CMake and link to the
`MagnumPlugins::ColladaImporter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.

@section Trade-ColladaImporter-limitations Behavior and limitations

@subsection Trade-ColladaImporter-limitations-meshes Mesh import

-   Only quad and triangle meshes are supported
-   More than one polygon list in a single mesh is not supported
-   Only vertex positions, normals and 2D texture coordinates are supported

@subsection Trade-ColladaImporter-limitations-materials Material import

-   Only the `COMMON` effect profile is supported
*/
class MAGNUM_COLLADAIMPORTER_EXPORT ColladaImporter: public AbstractImporter {
    public:
        /**
         * @brief Default constructor
         *
         * In case you want to open images, use
         * @ref ColladaImporter(PluginManager::Manager<AbstractImporter>&)
         * instead.
         */
        explicit ColladaImporter();

        /**
         * @brief Constructor
         *
         * The plugin needs access to plugin manager for importing images.
         */
        explicit ColladaImporter(PluginManager::Manager<AbstractImporter>& manager);

        /** @brief Plugin manager constructor */
        explicit ColladaImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~ColladaImporter();

    private:
        struct Document;

        MAGNUM_COLLADAIMPORTER_LOCAL Features doFeatures() const override;

        MAGNUM_COLLADAIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_COLLADAIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_COLLADAIMPORTER_LOCAL void doClose() override;

        MAGNUM_COLLADAIMPORTER_LOCAL Int doDefaultScene() override;
        MAGNUM_COLLADAIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_COLLADAIMPORTER_LOCAL Int doSceneForName(const std::string& name) override;
        MAGNUM_COLLADAIMPORTER_LOCAL std::string doSceneName(UnsignedInt id) override;
        MAGNUM_COLLADAIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_COLLADAIMPORTER_LOCAL UnsignedInt doObject3DCount() const override;
        MAGNUM_COLLADAIMPORTER_LOCAL Int doObject3DForName(const std::string& name) override;
        MAGNUM_COLLADAIMPORTER_LOCAL std::string doObject3DName(UnsignedInt id) override;
        MAGNUM_COLLADAIMPORTER_LOCAL std::unique_ptr<ObjectData3D> doObject3D(UnsignedInt id) override;

        MAGNUM_COLLADAIMPORTER_LOCAL UnsignedInt doMesh3DCount() const override;
        MAGNUM_COLLADAIMPORTER_LOCAL Int doMesh3DForName(const std::string& name) override;
        MAGNUM_COLLADAIMPORTER_LOCAL std::string doMesh3DName(UnsignedInt id) override;
        MAGNUM_COLLADAIMPORTER_LOCAL Containers::Optional<MeshData3D> doMesh3D(UnsignedInt id) override;

        MAGNUM_COLLADAIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_COLLADAIMPORTER_LOCAL Int doMaterialForName(const std::string& name) override;
        std::string doMaterialName(UnsignedInt id) override;
        MAGNUM_COLLADAIMPORTER_LOCAL std::unique_ptr<AbstractMaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_COLLADAIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_COLLADAIMPORTER_LOCAL Int doTextureForName(const std::string& name) override;
        MAGNUM_COLLADAIMPORTER_LOCAL std::string doTextureName(UnsignedInt id) override;
        MAGNUM_COLLADAIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_COLLADAIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_COLLADAIMPORTER_LOCAL Int doImage2DForName(const std::string& name) override;
        MAGNUM_COLLADAIMPORTER_LOCAL std::string doImage2DName(UnsignedInt id) override;
        MAGNUM_COLLADAIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id) override;

        /* Offset of attribute in mesh index array */
        MAGNUM_COLLADAIMPORTER_LOCAL UnsignedInt attributeOffset(UnsignedInt meshId, const QString& attribute, UnsignedInt id = 0);

        /* Build attribute array */
        template<class T> MAGNUM_COLLADAIMPORTER_LOCAL std::vector<T> buildAttributeArray(UnsignedInt meshId, const QString& attribute, UnsignedInt id, UnsignedInt stride, const std::vector<UnsignedInt>& interleavedIndexArrays);

        MAGNUM_COLLADAIMPORTER_LOCAL std::string instanceName(const QString& name, const QString& instanceTag);

        /* Default namespace declaration for XQuery */
        static const QString namespaceDeclaration;

        /* Currently opened document */
        Document* d;

        /* QCoreApplication needs pointer to 'argc', faking it by pointing here */
        int zero;

        /* QCoreApplication, which must be started in order to use QXmlQuery */
        QCoreApplication* app;
};

}}

#endif
