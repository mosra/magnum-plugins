/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Jonathan Hale <squareys@googlemail.com>

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
    FROM, OUT OF OR IN CONNETCION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

using namespace Magnum;

int main() {

{
PluginManager::Manager<Trade::AbstractImporter> manager;
/* [target-format-suffix] */
/* Choose ETC2 target format */
Containers::Pointer<Trade::AbstractImporter> importerEtc2 =
    manager.instantiate("BasisImporterEtc2");

/* Choose BC5 target format */
Containers::Pointer<Trade::AbstractImporter> importerBc5 =
    manager.instantiate("BasisImporterBc5");
/* [target-format-suffix] */
}

{
PluginManager::Manager<Trade::AbstractImporter> manager;
Containers::Optional<Trade::ImageData2D> image;
/* [target-format-config] */
/* Instantiate the plugin under its default name. At this point, the plugin
   can not import images yet as it doesn't know what it transcodes to. */
Containers::Pointer<Trade::AbstractImporter> importer =
    manager.instantiate("BasisImporter");
importer->openFile("mytexture.basis");

/* Transcode the image to BC5 */
importer->configuration().setValue("format", "Bc5");
image = importer->image2D(0);
// ...

/* Transcode the same image, but to ETC2 now */
importer->configuration().setValue("format", "Etc2");
image = importer->image2D(0);
// ...
/* [target-format-config] */
}

}
