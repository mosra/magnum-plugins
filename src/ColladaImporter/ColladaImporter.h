#ifndef Magnum_Plugins_ColladaImporter_ColladaImporter_h
#define Magnum_Plugins_ColladaImporter_ColladaImporter_h
/*
    Copyright © 2010, 2011, 2012 Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Magnum.

    Magnum is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Magnum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

/** @file
 * @brief Class Magnum::Plugins::ColladaImporter::ColladaImporter
 */

#include "AbstractImporter.h"

#include <unordered_map>
#include <QtCore/QCoreApplication>
#include <QtXmlPatterns/QXmlQuery>

#include "IndexedMesh.h"

namespace Magnum { namespace Plugins { namespace ColladaImporter {

class ColladaMeshData;

/**
@brief Collada importer plugin
*/
class ColladaImporter: public AbstractImporter {
    public:
        ColladaImporter(Corrade::PluginManager::AbstractPluginManager* manager = 0, const std::string& plugin = "");
        virtual ~ColladaImporter() { close(); }

        inline int features() const { return OpenFile; }

        bool open(const std::string& filename);
        void close();

        size_t objectCount() const { return d ? d->objects.size() : 0; }
        std::shared_ptr<Object> object(size_t id);

        size_t meshCount() const { return d ? d->meshes.size() : 0; }
        std::shared_ptr<MeshData> meshData(size_t meshId);
        std::shared_ptr<Mesh> mesh(size_t id);

        size_t materialCount() const { return d ? d->materials.size() : 0; }
        std::shared_ptr<AbstractMaterial> material(size_t id);

    private:
        /** @brief Contents of opened Collada document */
        struct Document {
            inline Document(size_t objectCount, size_t materialCount): objects(objectCount), meshData(objectCount), meshes(objectCount), materials(materialCount) {}

            /* Geometries and materials */
            std::vector<std::shared_ptr<Object>> objects;
            std::vector<std::shared_ptr<Plugins::ColladaImporter::ColladaMeshData>> meshData;
            std::vector<std::shared_ptr<Mesh>> meshes;
            std::vector<std::shared_ptr<AbstractMaterial>> materials;

            std::unordered_map<std::string, size_t> materialMap;

            QXmlQuery query;
        };

        /** @brief Per-vertex data */
        struct VertexData {
            Vector4 vertex;
            Vector3 normal;
        };

        /** @brief Default namespace declaration for XQuery */
        static const QString namespaceDeclaration;

        /** @brief Currently opened document */
        Document* d;

        /** @brief QCoreApplication needs pointer to 'argc', faking it by pointing here */
        int zero;

        /** @brief QCoreApplication, which must be started in order to use QXmlQuery */
        QCoreApplication app;

        /** @brief Parse &lt;source&gt; element */
        template<class T> std::vector<T> parseSource(const QString& id);

        /**
         * @brief Builder for index array based on index count
         *
         * @see SizeBasedCall
         */
        struct IndexBuilder {
            template<class IndexType> static void run(IndexedMesh* mesh, const std::vector<GLuint>& indices) {
                /* Compress index array. Using TypeTraits::IndexType to ensure
                   we have allowed type for indexing */
                std::vector<typename TypeTraits<IndexType>::IndexType> compressedIndices;
                compressedIndices.reserve(indices.size());
                for(auto it = indices.cbegin(); it != indices.cend(); ++it)
                    compressedIndices.push_back(*it);

                /* Update mesh parameters and fill index buffer */
                mesh->setIndexCount(compressedIndices.size());
                mesh->setIndexType(TypeTraits<IndexType>::glType());
                mesh->indexBuffer()->setData(sizeof(IndexType)*compressedIndices.size(), compressedIndices.data(), Buffer::DrawStatic);
            }
        };
};

}}}

#endif
