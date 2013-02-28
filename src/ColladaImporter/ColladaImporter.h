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
#include <Utility/MurmurHash2.h>

#include "ColladaType.h"
#include "Utility.h"

namespace Magnum { namespace Trade { namespace ColladaImporter {

class ColladaMeshData;

/**
@brief Collada importer plugin
*/
class ColladaImporter: public AbstractImporter {
    public:
        /** @copydoc AbstractImporter::AbstractImporter() */
        ColladaImporter(Corrade::PluginManager::AbstractPluginManager* manager = nullptr, const std::string& plugin = "");
        virtual ~ColladaImporter();

        inline Features features() const override { return Feature::OpenFile; }

        bool open(const std::string& filename) override;
        void close() override;

        Int defaultScene() override;
        inline UnsignedInt sceneCount() const override { return d ? d->scenes.size() : 0; }
        std::string sceneName(UnsignedInt id) override;
        SceneData* scene(UnsignedInt id) override;

        inline UnsignedInt object3DCount() const { return d ? d->objects.size() : 0; }
        Int object3DForName(const std::string& name) override;
        std::string object3DName(UnsignedInt id) override;
        ObjectData3D* object3D(UnsignedInt id) override;

        inline UnsignedInt mesh3DCount() const { return d ? d->meshes.size() : 0; }
        Int mesh3DForName(const std::string& name) override;
        std::string mesh3DName(UnsignedInt id) override;
        MeshData3D* mesh3D(UnsignedInt id) override;

        inline UnsignedInt materialCount() const override { return d ? d->materials.size() : 0; }
        Int materialForName(const std::string& name) override;
        std::string materialName(UnsignedInt id) override;
        AbstractMaterialData* material(UnsignedInt id) override;

        inline UnsignedInt image2DCount() const override { return d ? d->images2D.size() : 0; }
        Int image2DForName(const std::string& name) override;
        std::string image2DName(UnsignedInt id) override;
        ImageData2D* image2D(UnsignedInt id) override;

        /** @brief Parse &lt;source&gt; element */
        template<class T> std::vector<T> parseSource(const QString& id) {
            std::vector<T> output;
            QString tmp;

            /* Count of items */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
            d->query.evaluateTo(&tmp);
            UnsignedInt count = ColladaType<UnsignedInt>::fromString(tmp);

            /* Size of each item */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@stride/string()").arg(id));
            d->query.evaluateTo(&tmp);
            UnsignedInt size = ColladaType<UnsignedInt>::fromString(tmp);

            /* Data source */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@source/string()").arg(id));
            d->query.evaluateTo(&tmp);
            QString source = tmp.mid(1).trimmed();

            /* Verify total count */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/@count/string()").arg(source));
            d->query.evaluateTo(&tmp);
            if(ColladaType<UnsignedInt>::fromString(tmp) != count*size) {
                Corrade::Utility::Error() << "ColladaImporter: wrong total count in source" << ('"'+id+'"').toStdString();
                return output;
            }

            /** @todo Assert right order of coordinates and type */

            /* Items */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/string()").arg(source));
            d->query.evaluateTo(&tmp);

            output.reserve(count);
            Int from = 0;
            for(std::size_t i = 0; i != count; ++i)
                output.push_back(Utility::parseVector<T>(tmp, &from, size));

            return output;
        }

    private:
        /** @brief Contents of opened Collada document */
        struct Document {
            inline Document(): defaultScene(0) {}
            ~Document();

            std::string filename;

            /* Data */
            UnsignedInt defaultScene;
            std::vector<std::pair<std::string, SceneData*>> scenes;
            std::vector<std::pair<std::string, ObjectData3D*>> objects;
            std::vector<std::pair<std::string, MeshData3D*>> meshes;
            std::vector<std::pair<std::string, AbstractMaterialData*>> materials;
            std::vector<std::pair<std::string, ImageData2D*>> images2D;

            /** @todo Make public use for camerasForName, lightsForName */
            std::unordered_map<std::string, UnsignedInt> camerasForName,
                lightsForName,
                objectsForName,
                meshesForName,
                materialsForName,
                images2DForName;

            QXmlQuery query;
        };

        /** @brief %Mesh index hasher */
        class IndexHash {
            public:
                /** @brief Constructor */
                inline IndexHash(const std::vector<UnsignedInt>& indices, UnsignedInt stride): indices(indices), stride(stride) {}

                /**
                 * @brief Functor
                 *
                 * Computes hash for given index of length @c stride,
                 * specified as position in index array passed in
                 * constructor.
                 */
                inline std::size_t operator()(UnsignedInt key) const {
                    return *reinterpret_cast<const std::size_t*>(Corrade::Utility::MurmurHash2()(reinterpret_cast<const char*>(indices.data()+key*stride), sizeof(UnsignedInt)*stride).byteArray());
                }

            private:
                const std::vector<UnsignedInt>& indices;
                UnsignedInt stride;
        };

        /** @brief %Mesh index comparator */
        class IndexEqual {
            public:
                /** @brief Constructor */
                inline IndexEqual(const std::vector<UnsignedInt>& indices, UnsignedInt stride): indices(indices), stride(stride) {}

                /**
                 * @brief Functor
                 *
                 * Compares two index combinations of length @c stride,
                 * specified as position in index array, passed in
                 * constructor.
                 */
                inline bool operator()(UnsignedInt a, UnsignedInt b) const {
                    return memcmp(indices.data()+a*stride, indices.data()+b*stride, sizeof(UnsignedInt)*stride) == 0;
                }

            private:
                const std::vector<UnsignedInt>& indices;
                UnsignedInt stride;
        };

        /**
         * @brief Offset of attribute in mesh index array
         * @param meshId            %Mesh ID
         * @param attribute         Attribute
         * @param id                Attribute ID, if there are more than one
         *      attribute with the same name
         */
        UnsignedInt attributeOffset(UnsignedInt meshId, const QString& attribute, UnsignedInt id = 0);

        /**
         * @brief Build attribute array
         * @param meshId            %Mesh ID
         * @param attribute         Attribute
         * @param id                Attribute ID, if there are more than one
         *      attribute with the same name
         * @param originalIndices   Array with original interleaved indices
         * @param stride            Distance between two successive original
         *      indices
         * @param indexCombinations Index combinations for building the array
         * @return Resulting array
         */
        template<class T> std::vector<T>* buildAttributeArray(UnsignedInt meshId, const QString& attribute, UnsignedInt id, const std::vector<UnsignedInt>& originalIndices, UnsignedInt stride, const std::unordered_map<UnsignedInt, UnsignedInt, IndexHash, IndexEqual>& indexCombinations) {
            QString tmp;

            /* Original attribute array */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@source/string()")
                .arg(meshId+1).arg(attribute).arg(id+1));
            d->query.evaluateTo(&tmp);
            std::vector<T> originalArray = parseSource<T>(tmp.mid(1).trimmed());

            /* Attribute offset in original index array */
            UnsignedInt offset = attributeOffset(meshId, attribute, id);

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
        UnsignedInt parseObject(UnsignedInt id, const QString& name);

        /**
         * @brief Instance name
         * @param objectName    Object name
         * @param instanceTag   Instance tag name
         * @return Instance name
         */
        std::string instanceName(const QString& name, const QString& instanceTag);

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
