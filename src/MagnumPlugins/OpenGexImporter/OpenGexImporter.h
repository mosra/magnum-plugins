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

This plugin is built if `WITH_OPENGEXIMPORTER` is enabled when building Magnum
Plugins. To use dynamic plugin, you need to load `OpenGexImporter` plugin from
`MAGNUM_PLUGINS_IMPORTER_DIR`. To use static plugin, you need to request
`OpenGexImporter` component of `MagnumPlugins` package in CMake and link to
`${MAGNUMPLUGINS_OPENGEXIMPORTER_LIBRARIES}`. To use this as a dependency of
another plugin, you additionally need to add
`${MAGNUMPLUGINS_OPENGEXIMPORTER_INCLUDE_DIRS}` to include path. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

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
        void doOpenData(Containers::ArrayReference<const char> data) override;
        void doClose() override;

        std::unique_ptr<Document> _d;
};

}}

#endif
