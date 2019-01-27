#ifndef Magnum_Trade_TinyGltfImporter_h
#define Magnum_Trade_TinyGltfImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>

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
 * @brief Class @ref Magnum::Trade::TinyGltfImporter
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/TinyGltfImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
namespace tinygltf {
    class Model;
}
#endif

namespace Magnum { namespace Trade {

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC
    #ifdef TinyGltfImporter_EXPORTS
        #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_TINYGLTFIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_TINYGLTFIMPORTER_EXPORT
#define MAGNUM_TINYGLTFIMPORTER_LOCAL
#endif

/**
@brief TinyGltf importer plugin

@m_keywords{GltfImporter GlbImporter}

Imports glTF and binary glTF using the [TinyGLTF](https://github.com/syoyo/tinygltf)
library.

This plugin depends on the @ref Trade library and is built if
`WITH_TINYGLTFIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, you need to load the @cpp "TinyGltfImporter" @ce plugin from
`MAGNUM_PLUGINS_IMPORTER_DIR`. To use as a static plugin or as a dependency of
another plugin with CMake, you need to request the `TinyGltfImporter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::TinyGltfImporter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.

This plugin provides `GltfImporter` and `GlbImporter` plugins.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [TinyGLTF](https://github.com/syoyo/tinygltf)
    library, licensed under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/syoyo/tinygltf/blob/devel/LICENSE),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).
    It requires attribution for public use. TinyGLTF itself uses Niels
    Lohmann's [json.hpp](https://github.com/nlohmann/json), licensed under
    @m_class{m-label m-success} **MIT** as well
    ([license text](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT)).

@section Trade-TinyGltfImporter-limitations Behavior and limitations

The plugin supports @ref Feature::OpenData and @ref Feature::FileCallback
features. The `tiny_gltf` library loads everything during initial import,
meaning all external file loading callbacks are called with
@ref ImporterFileCallbackPolicy::LoadTemporary and the resources can be safely
freed right after the @ref openData() / @ref openFile() function exits. In case
of images, the files are loaded on-demand inside @ref image2D() calls with
@ref ImporterFileCallbackPolicy::LoadTemporary and @ref ImporterFileCallbackPolicy::Close
is emitted right after the file is fully read.

Import of skeleton, skin and morph data is not supported at the moment.

@subsection Trade-TinyGltfImporter-limitations-animation Animation import

-   Linear quaternion rotation tracks are postprocessed in order to make it
    possible to use the faster
    @ref Math::lerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerp()" /
    @ref Math::slerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerp()"
    functions instead of
    @ref Math::lerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerpShortestPath()" /
    @ref Math::slerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerpShortestPath()". Can be disabled per-animation with the
    @cb{.ini} optimizeQuaternionShortestPath @ce option, see
    @ref Trade-TinyGltfImporter-configuration "below". This doesn't affect
    spline-interpolated rotation tracks.
-   If linear quaternion rotation tracks are not normalized, the importer
    prints a warning and normalizes them. Can be disabled per-animation with
    the @cb{.ini} normalizeQuaternions @ce option, see
    @ref Trade-TinyGltfImporter-configuration "below". This doesn't affect
    spline-interpolated rotation tracks.
-   Skinning and morph targets are not supported
-   Animation tracks are always imported with
    @ref Animation::Extrapolation::Constant, because glTF doesn't support
    anything else
-   It's possible to request all animation clips to be merged into one using
    the @cb{.ini} mergeAnimationClips @ce option in order to for example
    preserve cinematic animations when using the Blender glTF exporter (as it
    otherwise outputs a separate clip for each object). When this option is
    enabled, @ref animationCount() always report either @cpp 0 @ce or
    @cpp 1 @ce and the merged animation has no name. With this option enabled,
    however, it can happen that multiple conflicting tracks affecting the same
    node are merged in the same clip, causing the animation to misbehave.

@subsection Trade-TinyGltfImporter-limitations-objects Scene and object import

-   If no @cb{.json} "scene" @ce property is present and the file contains at
    least one scene, @ref defaultScene() returns @cpp 0 @ce instead of
    @cpp -1 @ce. According to the [glTF 2.0 specification](https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#scenes)
    the importer is free to not render anything, but the suggested behavior
    would break even some official sample models.
-   In case object transformation is set via separate
    translation/rotation/scaling properties in the source file,
    @ref ObjectData3D is created with @ref ObjectFlag3D::HasTranslationRotationScaling and these separate properties accessible
-   If object rotation quaternion is not normalized, the importer prints a
    warning and normalizes it. Can be disabled per-object with the
    @cb{.ini} normalizeQuaternions @ce option, see
    @ref Trade-TinyGltfImporter-configuration "below".

@subsection Trade-TinyGltfImporter-limitations-camera Camera import

-   Cameras in glTF are specified with vertical FoV and vertical:horizontal
    aspect ratio, these values are recalculated for horizontal FoV and
    horizontal:vertical aspect ratio as is common in Magnum

@subsection Trade-TinyGltfImporter-limitations-lights Light import

-   Currently, the importer only supports the historical `KHR_lights_cmn`
    extension, supported by the original Khronos Blender glTF Exporter.
    -   Light properties besides light type are not imported.
    -   Light intensity is not yet supported due to glTF extension draft state.

@subsection Trade-TinyGltfImporter-limitations-meshes Mesh import

-   Meshes with interleaved vertex data/buffer views are not supported.
-   Multi-primitive meshes are loaded as follows:
    -   The @ref mesh3DCount() query returns a number of all *primitives*, not
        meshes
    -   Each multi-primitive mesh is split into a sequence of @ref MeshData3D
        instances following each other
    -   @ref mesh3DForName() points to the first mesh in given sequence and
        @ref mesh3DName() returns the same name for all meshes in given
        sequence
    -   The @ref object3DCount() query returns a number of all nodes extended
        with number of extra nodes for each additional mesh primitive
    -   Each node referencing a multi-primitive mesh is split into a sequence
        of @ref MeshObjectData3D instances following each other; the extra
        nodes being a direct and immediate children of the first one with an
        identity transformation
    -   @ref object3DForName() points to the first object containing the first
        primitive, @ref object3DName() returns the same name for all objects in
        given sequence
    -   @ref AnimationData instances returned by @ref animation() have their
        @ref AnimationData::trackTarget() values patched to account for the
        extra nodes, always pointing to the first object in the sequence and
        thus indirectly affecting transformations of the extra nodes
        represented as its children

@subsection Trade-TinyGltfImporter-limitations-materials Material import

-   Subset of all material specs is currently imported as @ref PhongMaterialData
-   Ambient color is always @cpp 0x000000_rgbf @ce (never a texture)
-   For the Metallic/Roughness material spec ([in core](https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#metallic-roughness-material),
    default):
    -   The `baseColorTexture` field is used for diffuse texture, if present.
        Otherwise, `baseColorFactor` is used for diffuse color, if present.
        Otherwise, @cpp 0xffffff_rgbf @ce is used.
    -   Specular color is always @cpp 0xffffff_rgbf @ce (never a texture)
    -   Shininess is always @cpp 1.0f @ce
-   For the Specular/Glossiness material spec
    ([KHR_materials_pbrSpecularGlossiness](https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness)
    extension):
    -   The `diffuseTexture` field is used for diffuse texture, if present.
        Otherwise, `diffuseFactor` is used for diffuse color, if present.
        Otherwise, @cpp 0xffffff_rgbf @ce is used.
    -   The `specularGlossinessTexture` field is used for a specular texture,
        if present (note that only the RGB channels should be used and the
        alpha channel --- containing glossiness --- should be ignored).
        Otherwise, `specularFactor` is used for specular color, if present.
        Otherwise, @cpp 0xffffff_rgbf @ce is used.
    -   Shininess is always @cpp 1.0f @ce
-   For the Blinn/Phong material spec (*draft*
    [KHR_materials_cmnBlinnPhong](https://github.com/KhronosGroup/glTF/tree/gltf2-common-materials/extensions/Khronos/KHR_materials_cmnBlinnPhong)
    extension):
    -   The `diffuseTexture` field is used for diffuse texture, if present.
        Otherwise, `diffuseFactor` is used for diffuse color, if present.
        Otherwise, @cpp 0xffffff_rgbf @ce is used.
    -   The `specularShininessTexture` field is used for specular texture, if
        present (note that only the RGB channels should be used and the alpha
        channel --- containing snininess --- should be ignored). Otherwise,
        `specularFactor` is used for diffuse color, if present. Otherwise,
        @cpp 0xffffff_rgbf @ce is used.
    -   The `snininessFactor` field is used for snininess, if present.
        Otherwise @cpp 1.0f @ce is used.

@subsection Trade-TinyGltfImporter-limitations-textures Texture import

-   Texture type is always @ref Trade::TextureData::Type::Texture2D, as glTF
    doesn't support anything else
-   Z coordinate of @ref Trade::TextureData::wrapping() is always
    @ref SamplerWrapping::Repeat, as glTF doesn't support 3D textures
-   glTF leaves the defaults of sampler properties to the application, the
    following defaults have been chosen for this importer:
    -   Minification/magnification/mipmap filter: @ref SamplerFilter::Linear,
        @ref SamplerMipmap::Linear
    -   Wrapping (all axes): @ref SamplerWrapping::Repeat

@section Trade-TinyGltfImporter-configuration Plugin-specific config

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values.

@snippet MagnumPlugins/TinyGltfImporter/TinyGltfImporter.conf config

@section Trade-TinyGltfImporter-state Access to internal importer state

Access to the underlying TinyGLTF structures it is provided through
importer-specific data accessors:

-   @ref importerState() returns pointer to the `tinygltf::Model` structure.
    If you use this class statically, you get the concrete type instead of
    a @cpp const void* @ce pointer as returned by
    @ref AbstractImporter::importerState().
-   @ref AbstractMaterialData::importerState() returns pointer to the
    `tinygltf::Material` structure
-   @ref CameraData::importerState() returns pointer to the `tinygltf::Camera`
    structure
-   @ref ImageData::importerState() returns pointer to the `tinygltf::Image`
    structure
-   @ref MeshData3D::importerState() returns pointer to the `tinygltf::Mesh`
    structure
-   @ref ObjectData3D::importerState() returns pointer to the `tinygltf::Node`
    structure
-   @ref SceneData::importerState() returns pointer to the `tinygltf::Scene`
    structure
-   @ref TextureData::importerState() returns pointer to the
    `tinygltf::Texture` structure

The TinyGLTF header is installed alsongside the plugin and accessible like
this:

@code{.cpp}
#include <MagnumExternal/TinyGLTF/tiny_gltf.h>
@endcode
*/
class MAGNUM_TINYGLTFIMPORTER_EXPORT TinyGltfImporter: public AbstractImporter {
    public:
        /**
         * @brief Default constructor
         *
         * In case you want to open images, use
         * @ref TinyGltfImporter(PluginManager::Manager<AbstractImporter>&)
         * instead.
         */
        explicit TinyGltfImporter();

        /**
         * @brief Constructor
         *
         * The plugin needs access to plugin manager for importing images.
         */
        explicit TinyGltfImporter(PluginManager::Manager<AbstractImporter>& manager);

        /** @brief Plugin manager constructor */
        explicit TinyGltfImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~TinyGltfImporter();

        /**
         * @brief Importer state
         *
         * See @ref Trade-TinyGltfImporter-state "class documentation" for more
         * information.
         */
        const tinygltf::Model* importerState() const {
            return static_cast<const tinygltf::Model*>(AbstractImporter::importerState());
        }

    private:
        struct Document;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Features doFeatures() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL bool doIsOpened() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL void doClose() override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doAnimationCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doAnimationName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doAnimationForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<AnimationData> doAnimation(UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doCameraForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doCameraName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doCameraCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doLightForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doLightName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doLightCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doDefaultScene() override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doSceneForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doSceneName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doSceneCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doObject3DCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doObject3DForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doObject3DName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Pointer<ObjectData3D> doObject3D(UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doMesh3DCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doMesh3DForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doMesh3DName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<MeshData3D> doMesh3D(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doMaterialForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doMaterialName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Pointer<AbstractMaterialData> doMaterial(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doTextureForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doTextureName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doImage2DForName(const std::string& name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL std::string doImage2DName(const UnsignedInt) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(const UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL const void* doImporterState() const override;

        Containers::Pointer<Document> _d;
};

}}

#endif
