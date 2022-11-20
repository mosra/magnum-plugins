#ifndef Magnum_Trade_UfbxImporter_h
#define Magnum_Trade_UfbxImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Samuli Raivio <bqqbarbhg@gmail.com>

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
 * @brief Class @ref Magnum::Trade::UfbxImporter
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/UfbxImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_UFBXIMPORTER_BUILD_STATIC
    #ifdef UfbxImporter_EXPORTS
        #define MAGNUM_UFBXIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_UFBXIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_UFBXIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_UFBXIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_UFBXIMPORTER_EXPORT
#define MAGNUM_UFBXIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

class MAGNUM_UFBXIMPORTER_EXPORT UfbxImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit UfbxImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~UfbxImporter();

    private:
        MAGNUM_UFBXIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_UFBXIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_UFBXIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_UFBXIMPORTER_LOCAL void doOpenFile(Containers::StringView filename) override;
        MAGNUM_UFBXIMPORTER_LOCAL void doOpenState(const void* state, Containers::StringView filePath) override;
        MAGNUM_UFBXIMPORTER_LOCAL void doClose() override;

        MAGNUM_UFBXIMPORTER_LOCAL Int doDefaultScene() const override;
        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL const void* doImporterState() const override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedLong doObjectCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Long doObjectForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doObjectName(UnsignedLong id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doCameraCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doCameraForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doCameraName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doLightCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doLightForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doLightName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doMaterialForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doMaterialName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<MaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doTextureForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doTextureName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL AbstractImporter* setupOrReuseImporterForImage(UnsignedInt id, const char* errorPrefix);

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
