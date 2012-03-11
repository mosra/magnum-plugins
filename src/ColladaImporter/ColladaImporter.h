#ifndef Magnum_Trade_ColladaImporter_ColladaImporter_h
#define Magnum_Trade_ColladaImporter_ColladaImporter_h
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
 * @brief Class Magnum::Trade::ColladaImporter::ColladaImporter
 */

#include "Trade/AbstractImporter.h"

#include <unordered_map>
#include <QtCore/QCoreApplication>
#include <QtXmlPatterns/QXmlQuery>

#include "Utility/MurmurHash2.h"
#include "ColladaType.h"
#include "Utility.h"

namespace Magnum { namespace Trade { namespace ColladaImporter {

class ColladaMeshData;

/**
@brief Collada importer plugin
*/
class ColladaImporter: public AbstractImporter {
    public:
        ColladaImporter(Corrade::PluginManager::AbstractPluginManager* manager = 0, const std::string& plugin = "");
        virtual ~ColladaImporter();

        inline int features() const { return OpenFile; }

        bool open(const std::string& filename);
        void close();

        size_t sceneCount() const { return d ? d->scenes.size() : 0; }
        SceneData* scene(size_t id);

        size_t objectCount() const { return d ? d->objects.size() : 0; }
        ObjectData* object(size_t id);

        size_t meshCount() const { return d ? d->meshes.size() : 0; }
        MeshData* mesh(size_t id);

        size_t materialCount() const { return d ? d->materials.size() : 0; }
        AbstractMaterialData* material(size_t id);

        size_t image2DCount() const { return d ? d->images2D.size() : 0; }
        ImageData2D* image2D(size_t id);

        /** @brief Parse &lt;source&gt; element */
        template<class T> std::vector<T> parseSource(const QString& id) {
            std::vector<T> output;
            QString tmp;

            /* Count of items */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
            d->query.evaluateTo(&tmp);
            GLuint count = ColladaType<GLuint>::fromString(tmp);

            /* Size of each item */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@stride/string()").arg(id));
            d->query.evaluateTo(&tmp);
            GLuint size = ColladaType<GLuint>::fromString(tmp);

            /* Data source */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@source/string()").arg(id));
            d->query.evaluateTo(&tmp);
            QString source = tmp.mid(1).trimmed();

            /* Verify total count */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/@count/string()").arg(source));
            d->query.evaluateTo(&tmp);
            if(ColladaType<GLuint>::fromString(tmp) != count*size) {
                Corrade::Utility::Error() << "ColladaImporter: wrong total count in source" << ('"'+id+'"').toStdString();
                return output;
            }

            /** @todo Assert right order of coordinates and type */

            /* Items */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/string()").arg(source));
            d->query.evaluateTo(&tmp);

            output.reserve(count);
            int from = 0;
            for(size_t i = 0; i != count; ++i)
                output.push_back(Utility::parseVector<T>(tmp, &from, size));

            return output;
        }

    private:
        /** @brief Contents of opened Collada document */
        struct Document {
            inline Document(size_t sceneCount, size_t objectCount, size_t meshCount, size_t materialCount, size_t image2DCount): scenes(sceneCount), objects(objectCount), meshes(meshCount), materials(materialCount), images2D(image2DCount) {}

            ~Document();

            std::string filename;

            /* Data */
            std::vector<SceneData*> scenes;
            std::vector<ObjectData*> objects;
            std::vector<MeshData*> meshes;
            std::vector<AbstractMaterialData*> materials;
            std::vector<ImageData2D*> images2D;

            QXmlQuery query;
        };

        /** @brief Mesh index hasher */
        class IndexHash {
            public:
                /** @brief Constructor */
                inline IndexHash(const std::vector<unsigned int>& indices, unsigned int stride): indices(indices), stride(stride) {}

                /**
                 * @brief Functor
                 *
                 * Computes hash for given index of length @c stride,
                 * specified as position in index array passed in
                 * constructor.
                 */
                inline size_t operator()(unsigned int key) const {
                    return *reinterpret_cast<const size_t*>(Corrade::Utility::MurmurHash2()(reinterpret_cast<const char*>(indices.data()+key*stride), sizeof(unsigned int)*stride).byteArray());
                }

            private:
                const std::vector<unsigned int>& indices;
                unsigned int stride;
        };

        /** @brief Mesh index comparator */
        class IndexEqual {
            public:
                /** @brief Constructor */
                inline IndexEqual(const std::vector<unsigned int>& indices, unsigned int stride): indices(indices), stride(stride) {}

                /**
                 * @brief Functor
                 *
                 * Compares two index combinations of length @c stride,
                 * specified as position in index array, passed in
                 * constructor.
                 */
                inline bool operator()(unsigned int a, unsigned int b) const {
                    return memcmp(indices.data()+a*stride, indices.data()+b*stride, sizeof(unsigned int)*stride) == 0;
                }

            private:
                const std::vector<unsigned int>& indices;
                unsigned int stride;
        };

        /**
         * @brief Offset of attribute in mesh index array
         * @param meshId            Mesh ID
         * @param attribute         Attribute
         * @param id                Attribute ID, if there are more than one
         *      attribute with the same name
         */
        GLuint attributeOffset(size_t meshId, const QString& attribute, unsigned int id = 0);

        /**
         * @brief Build attribute array
         * @param meshId            Mesh ID
         * @param attribute         Attribute
         * @param id                Attribute ID, if there are more than one
         *      attribute with the same name
         * @param originalIndices   Array with original interleaved indices
         * @param stride            Distance between two successive original
         *      indices
         * @param indexCombinations Index combinations for building the array
         * @return Resulting array
         */
        template<class T> std::vector<T>* buildAttributeArray(size_t meshId, const QString& attribute, unsigned int id, const std::vector<GLuint>& originalIndices, GLuint stride, const std::unordered_map<unsigned int, unsigned int, IndexHash, IndexEqual>& indexCombinations) {
            QString tmp;

            /* Original attribute array */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@source/string()")
                .arg(meshId+1).arg(attribute).arg(id+1));
            d->query.evaluateTo(&tmp);
            std::vector<T> originalArray = parseSource<T>(tmp.mid(1).trimmed());

            /* Attribute offset in original index array */
            GLuint offset = attributeOffset(meshId, attribute, id);

            /* Build resulting array */
            std::vector<T>* array = new std::vector<T>(indexCombinations.size());
            for(auto i: indexCombinations)
                (*array)[i.second] = originalArray[originalIndices[i.first*stride+offset]];

            return array;
        }

        /** @brief Parse all scenes */
        void parseScenes();

        /**
         * @brief Parse object
         * @param id        Object ID, under which it will be saved
         * @param name      Object name
         * @return Next free ID
         */
        size_t parseObject(size_t id, const QString& name);

        /** @brief Default namespace declaration for XQuery */
        static const QString namespaceDeclaration;

        /** @brief Currently opened document */
        Document* d;

        /** @brief QCoreApplication needs pointer to 'argc', faking it by pointing here */
        int zero;

        /** @brief QCoreApplication, which must be started in order to use QXmlQuery */
        QCoreApplication* app;
};

}}}

#endif
