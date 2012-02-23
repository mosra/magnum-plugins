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

        size_t meshCount() const { return d ? d->meshes.size() : 0; }
        MeshData* mesh(size_t id);

        size_t materialCount() const { return d ? d->materials.size() : 0; }
        AbstractMaterialData* material(size_t id);

        /** @brief Parse &lt;source&gt; element */
        template<class T> std::vector<T> parseSource(const QString& id) {
            std::vector<T> output;
            QString tmp;

            /* Count of items */
            d->query.setQuery((namespaceDeclaration + "//source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
            d->query.evaluateTo(&tmp);
            GLuint count = ColladaType<GLuint>::fromString(tmp);

            /* Size of each item */
            d->query.setQuery((namespaceDeclaration + "//source[@id='%0']/technique_common/accessor/@stride/string()").arg(id));
            d->query.evaluateTo(&tmp);
            GLuint size = ColladaType<GLuint>::fromString(tmp);

            /* Data source */
            d->query.setQuery((namespaceDeclaration + "//source[@id='%0']/technique_common/accessor/@source/string()").arg(id));
            d->query.evaluateTo(&tmp);
            QString source = tmp.mid(1).trimmed();

            /* Verify total count */
            d->query.setQuery((namespaceDeclaration + "//float_array[@id='%0']/@count/string()").arg(source));
            d->query.evaluateTo(&tmp);
            if(ColladaType<GLuint>::fromString(tmp) != count*size) {
                Corrade::Utility::Error() << "ColladaImporter: wrong total count in source" << ('"'+id+'"').toStdString();
                return output;
            }

            /** @todo Assert right order of coordinates and type */

            /* Items */
            d->query.setQuery((namespaceDeclaration + "//float_array[@id='%0']/string()").arg(source));
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
            inline Document(size_t objectCount, size_t materialCount): meshes(objectCount), materials(materialCount) {}

            ~Document();

            /* Geometries and materials */
            std::vector<MeshData*> meshes;
            std::vector<AbstractMaterialData*> materials;

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
        QCoreApplication* app;
};

}}}

#endif
