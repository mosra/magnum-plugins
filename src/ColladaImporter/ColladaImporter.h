#ifndef Magnum_Trade_ColladaImporter_h
#define Magnum_Trade_ColladaImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013 Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class Magnum::Trade::ColladaImporter
 */

#include "Trade/AbstractImporter.h"

#include <unordered_map>
#include <QtCore/QCoreApplication>
#include <QtXmlPatterns/QXmlQuery>
#include <Utility/MurmurHash2.h>

#include "ColladaType.h"
#include "Utility.h"

namespace Magnum { namespace Trade {

class ColladaMeshData;

/**
@brief Collada importer plugin
*/
class ColladaImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit ColladaImporter();

        /** @brief Plugin manager constructor */
        explicit ColladaImporter(PluginManager::AbstractManager* manager, std::string plugin);

        ~ColladaImporter();

        /** @brief Parse &lt;source&gt; element */
        template<class T> std::vector<T> parseSource(const QString& id) {
            std::vector<T> output;
            QString tmp;

            /* Count of items */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
            d->query.evaluateTo(&tmp);
            UnsignedInt count = Implementation::ColladaType<UnsignedInt>::fromString(tmp);

            /* Size of each item */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@stride/string()").arg(id));
            d->query.evaluateTo(&tmp);
            UnsignedInt size = Implementation::ColladaType<UnsignedInt>::fromString(tmp);

            /* Data source */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@source/string()").arg(id));
            d->query.evaluateTo(&tmp);
            QString source = tmp.mid(1).trimmed();

            /* Verify total count */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/@count/string()").arg(source));
            d->query.evaluateTo(&tmp);
            if(Implementation::ColladaType<UnsignedInt>::fromString(tmp) != count*size) {
                Error() << "ColladaImporter: wrong total count in source" << ('"'+id+'"').toStdString();
                return output;
            }

            /** @todo Assert right order of coordinates and type */

            /* Items */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/string()").arg(source));
            d->query.evaluateTo(&tmp);

            output.reserve(count);
            Int from = 0;
            for(std::size_t i = 0; i != count; ++i)
                output.push_back(Implementation::Utility::parseVector<T>(tmp, &from, size));

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
            std::vector<std::string> meshes;
            std::vector<std::string> materials;
            std::vector<std::string> images2D;

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

        Features doFeatures() const override;

        bool doIsOpened() const override;
        void doOpenFile(const std::string& filename) override;
        void doClose() override;

        Int doDefaultScene() override;
        UnsignedInt doSceneCount() const override;
        std::string doSceneName(UnsignedInt id) override;
        SceneData* doScene(UnsignedInt id) override;

        UnsignedInt doObject3DCount() const override;
        Int doObject3DForName(const std::string& name) override;
        std::string doObject3DName(UnsignedInt id) override;
        ObjectData3D* doObject3D(UnsignedInt id) override;

        UnsignedInt doMesh3DCount() const override;
        Int doMesh3DForName(const std::string& name) override;
        std::string doMesh3DName(UnsignedInt id) override;
        MeshData3D* doMesh3D(UnsignedInt id) override;

        UnsignedInt doMaterialCount() const override;
        Int doMaterialForName(const std::string& name) override;
        std::string doMaterialName(UnsignedInt id) override;
        AbstractMaterialData* doMaterial(UnsignedInt id) override;

        UnsignedInt doImage2DCount() const override;
        Int doImage2DForName(const std::string& name) override;
        std::string doImage2DName(UnsignedInt id) override;
        ImageData2D* doImage2D(UnsignedInt id) override;

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
        template<class T> std::vector<T> buildAttributeArray(UnsignedInt meshId, const QString& attribute, UnsignedInt id, const std::vector<UnsignedInt>& originalIndices, UnsignedInt stride, const std::unordered_map<UnsignedInt, UnsignedInt, IndexHash, IndexEqual>& indexCombinations) {
            QString tmp;

            /* Original attribute array */
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@source/string()")
                .arg(meshId+1).arg(attribute).arg(id+1));
            d->query.evaluateTo(&tmp);
            std::vector<T> originalArray = parseSource<T>(tmp.mid(1).trimmed());

            /* Attribute offset in original index array */
            UnsignedInt offset = attributeOffset(meshId, attribute, id);

            /* Build resulting array */
            std::vector<T> array(indexCombinations.size());
            for(auto i: indexCombinations)
                array[i.second] = originalArray[originalIndices[i.first*stride+offset]];

            return std::move(array);
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

}}

#endif
