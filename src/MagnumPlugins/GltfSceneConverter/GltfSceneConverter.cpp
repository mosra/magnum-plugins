/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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

#include "GltfSceneConverter.h"

#include <cctype> /* std::isupper() */
#include <algorithm> /* std::sort() */
#include <unordered_map>
#include <Corrade/Containers/ArrayTuple.h>
#include <Corrade/Containers/ArrayViewStl.h> /** @todo drop once Configuration is STL-free */
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/BitArrayView.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Iterable.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Containers/StridedBitArrayView.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/JsonWriter.h>
#include <Corrade/Utility/Macros.h> /* CORRADE_UNUSED */
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/FunctionsBatch.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/ArrayAllocator.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/PbrClearCoatMaterialData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/SceneData.h>

#include "Magnum/Implementation/formatPluginsVersion.h"
#include "MagnumPlugins/GltfImporter/Gltf.h"

/* We'd have to endian-flip everything that goes into buffers, plus the binary
   glTF headers, etc. Too much work, hard to automatically test because the
   HW is hard to get. */
#ifdef CORRADE_TARGET_BIG_ENDIAN
#error this code will not work on Big Endian, sorry
#endif

namespace Magnum { namespace Trade {

namespace {

/* Each value here needs a corresponding entry in extensionStrings inside
   doEndData(), texture format extensions then detection based on MIME type in
   doAdd() for images and conversion to an extension name in doAdd() for
   textures. Values sorted by name. */
enum class GltfExtension {
    ExtTextureWebP = 1 << 0,
    KhrMaterialsClearCoat = 1 << 1,
    KhrMaterialsUnlit = 1 << 2,
    KhrMeshQuantization = 1 << 3,
    KhrTextureBasisu = 1 << 4,
    KhrTextureKtx = 1 << 5,
    KhrTextureTransform = 1 << 6,
};
typedef Containers::EnumSet<GltfExtension> GltfExtensions;
#ifdef CORRADE_TARGET_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
CORRADE_ENUMSET_OPERATORS(GltfExtensions)
#ifdef CORRADE_TARGET_CLANG
#pragma clang diagnostic pop
#endif

struct MeshProperties {
    Containers::Optional<UnsignedInt> gltfMode;
    /* Unfortunately we can't have a StringView here because the name can be
       composed out of a base and numeric suffix */
    Containers::Array<Containers::Pair<Containers::String, UnsignedInt>> gltfAttributes;
    Containers::Optional<UnsignedInt> gltfIndices;
    Containers::String gltfName;
};

}

struct GltfSceneConverter::State {
    /* Empty if saving to data. Storing the full filename and not just the path
       in order to know how to name the external buffer file. */
    Containers::Optional<Containers::StringView> filename;
    /* Custom mesh attribute names */
    Containers::Array<Containers::Pair<MeshAttribute, Containers::String>> customMeshAttributes;
    /* Object names */
    Containers::Array<Containers::String> objectNames;
    /* Scene field names. Can't use SceneField as the key because GCC 4.8
       doesn't have std::hash implemented for enums, F.F.S. */
    std::unordered_map<UnsignedInt, Containers::String> sceneFieldNames;
    /* Unique texture samplers. Key is packing all sampler properties, value is
       the output glTF sampler index. */
    std::unordered_map<UnsignedInt, UnsignedInt> uniqueSamplers;

    /* Output format. Defaults for a binary output. */
    bool binary = true;
    Utility::JsonWriter::Options jsonOptions;
    UnsignedInt jsonIndentation = 0;

    /* Extensions used / required based on data added. These two are mutually
       exclusive, what's in requiredExtensions shouldn't be in usedExtensions
       as well. */
    GltfExtensions usedExtensions, requiredExtensions;
    /* Extension strings added by custom material layers. Those are always
       only used, required only if the user explicitly adds them to the
       extensionRequired configuration option. */
    Containers::Array<Containers::String> usedCustomExtensions;

    /* Because in glTF a material is tightly coupled with a mesh instead of
       being only assigned from a scene node, all meshes go to this array first
       and are written to the file together with a material assignment at the
       end.

       If a mesh is referenced from a scene, it goes into
       `meshMaterialAssignments`, where the first is index into the meshes
       array and second is the material (or -1 if no material). In order to
       support multiple mesh assignments per objects, the assignments are first
       sorted, added one by one to `meshMaterialAssignments`, and then the
       final size of `meshMaterialAssignments` is appended to
       `meshMaterialAssignmentRanges`. Thus there item `i` to `i + 1` denote
       the set of meshes for glTF mesh ID `i`, which is then referenced by the
       scene.

       Meshes not referenced in the scene are not referenced from
       `meshMaterialAssignments` and are written at the very end. */
    Containers::Array<MeshProperties> meshes;
    Containers::Array<Containers::Pair<UnsignedInt, Int>> meshMaterialAssignments;
    Containers::Array<UnsignedInt> meshMaterialAssignmentRanges;

    /* For each 2D image contains its index in the gltfImages array (which is
       used for referencing from a texture) and a texture extension if needed
       (or GltfExtension{} if none). For each image that gets referenced by a
       texture, the extension is added to requiredExtensions. If an image isn't
       referenced by a texture, no extension is added. Size of the array is
       equal to image2DCount(). */
    Containers::Array<Containers::Pair<UnsignedInt, GltfExtension>> image2DIdsTextureExtensions;
    /* For each 3D image contains its index in the gltfImages array, a texture
       extension if needed, plus layer count (which is used to duplicate the
       texture referencing it, once for each layer). Size of the array is equal
       to image3DCount(). */
    Containers::Array<Containers::Triple<UnsignedInt, GltfExtension, UnsignedInt>> image3DIdsTextureExtensionsLayerCount;
    /* If a material references input texture i and layer j,
       `textureIdOffsets[i] + j` is the actual glTF texture ID to be written
       to the output. If only 2D images are present,
       `textureIdOffsets[i] == i` for all `i`. */
    Containers::Array<UnsignedInt> textureIdOffsets{InPlaceInit, {0}};

    Utility::JsonWriter gltfBuffers;
    Utility::JsonWriter gltfBufferViews;
    Utility::JsonWriter gltfAccessors;
    Utility::JsonWriter gltfNodes;
    Utility::JsonWriter gltfScenes;
    Utility::JsonWriter gltfMaterials;
    Utility::JsonWriter gltfSamplers;
    Utility::JsonWriter gltfTextures;
    Utility::JsonWriter gltfImages;

    Int defaultScene = -1;

    Containers::Array<char> buffer;
};

using namespace Containers::Literals;
using namespace Math::Literals;

GltfSceneConverter::GltfSceneConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractSceneConverter{manager, plugin} {}

GltfSceneConverter::~GltfSceneConverter() = default;

SceneConverterFeatures GltfSceneConverter::doFeatures() const {
    SceneConverterFeatures features =
        SceneConverterFeature::ConvertMultipleToData|
        SceneConverterFeature::AddScenes|
        SceneConverterFeature::AddMeshes|
        SceneConverterFeature::AddMaterials|
        SceneConverterFeature::AddTextures|
        SceneConverterFeature::AddImages2D|
        SceneConverterFeature::AddCompressedImages2D;
    /* Advertise 3D image support only if the experimental KHR_texture_ktx
       is enabled, for simpler error checking in add(ImageData3D) */
    if(configuration().value<bool>("experimentalKhrTextureKtx")) features |=
        SceneConverterFeature::AddImages3D|
        SceneConverterFeature::AddCompressedImages3D;
    return features;
}

bool GltfSceneConverter::doBeginFile(const Containers::StringView filename) {
    CORRADE_INTERNAL_ASSERT(!_state);
    _state.emplace();
    _state->filename = filename;

    /* Decide if we're writing a text or a binary file */
    if(!configuration().value<Containers::StringView>("binary")) {
        _state->binary = Utility::String::lowercase(Utility::Path::splitExtension(filename).second()) != ".gltf"_s;
    } else _state->binary = configuration().value<bool>("binary");

    return AbstractSceneConverter::doBeginFile(filename);
}

bool GltfSceneConverter::doBeginData() {
    /* If the state is already there, it's from doBeginFile(). Otherwise create
       a new one. */
    if(!_state) {
        _state.emplace();

        /* Binary is the default for data output because we can't write
           external files. Override if the configuration is non-empty. */
        if(!configuration().value<Containers::StringView>("binary"))
            _state->binary = true;
        else
            _state->binary = configuration().value<bool>("binary");
    }

    /* Text file is pretty-printed according to options. For a binary file the
       defaults are already alright.  */
    if(!_state->binary) {
        _state->jsonOptions = Utility::JsonWriter::Option::Wrap|Utility::JsonWriter::Option::TypographicalSpace;
        _state->jsonIndentation = 2;

        /* Update the JSON writers with desired options. These will be inside
           the top-level object, so need one level of initial indentation. */
        for(Utility::JsonWriter* const writer: {
            &_state->gltfBuffers,
            &_state->gltfBufferViews,
            &_state->gltfAccessors,
            &_state->gltfNodes,
            &_state->gltfScenes,
            &_state->gltfMaterials,
            &_state->gltfSamplers,
            &_state->gltfTextures,
            &_state->gltfImages
        })
            *writer = Utility::JsonWriter{_state->jsonOptions, _state->jsonIndentation, _state->jsonIndentation*1};
    }

    return true;
}

Containers::Optional<Containers::Array<char>> GltfSceneConverter::doEndData() {
    Utility::JsonWriter json{_state->jsonOptions, _state->jsonIndentation};
    json.beginObject();

    /* Asset object, always present */
    {
        json.writeKey("asset"_s);
        const Containers::ScopeGuard gltfAsset = json.beginObjectScope();

        json.writeKey("version"_s).write("2.0"_s);

        if(const Containers::StringView copyright = configuration().value<Containers::StringView>("copyright"_s))
            json.writeKey("copyright"_s).write(copyright);
        if(const Containers::StringView generator = configuration().value<Containers::StringView>("generator"_s)) {
            /* If the generator string contains a {0}, it'll get replaced. If
               it doesn't, it won't. It won't cause a security bug in that case
               -- this isn't printf(), it's safe to use unsanitized inputs as
               format templates. Worst case it'll assert if the user tries to
               use some invalid format pattern, but that's their problem, not
               mine. */
            json.writeKey("generator"_s).write(Utility::format(
                Containers::String::nullTerminatedView(generator).data(),
                Magnum::Implementation::formatPluginsVersion()
            ));
        }
    }

    /* Used and required extensions */
    {
        /** @todo FFS what the stone age types here */
        std::vector<Containers::StringView> extensionsUsed = configuration().values<Containers::StringView>("extensionUsed");
        std::vector<Containers::StringView> extensionsRequired = configuration().values<Containers::StringView>("extensionRequired");

        /** @todo use some set container instead once we have a variant that
            doesn't allocate like mad */
        const auto contains = [](Containers::ArrayView<const Containers::StringView> extensions, Containers::StringView extension) {
            for(const Containers::StringView i: extensions)
                if(i == extension)
                    return true;
            return false;
        };

        /* Add extensions used by custom material layers */
        for(const Containers::String& i: _state->usedCustomExtensions) {
            if(!contains(extensionsUsed, i))
                extensionsUsed.push_back(i);
        }

        /* To avoid issues where an extension would accidentally get added only
           to the required extension list but not used, the used list
           implicitly inherits all required extensions. For clean code, an
           extension should be either in the used list or in the required list,
           never in both. */
        CORRADE_INTERNAL_ASSERT(!(_state->usedExtensions & _state->requiredExtensions));
        /* Mutable in order to check that we didn't forget to handle any after
           the loop */
        GltfExtensions usedExtensions = _state->usedExtensions|_state->requiredExtensions;
        const Containers::Pair<GltfExtension, Containers::StringView> extensionStrings[]{
            {GltfExtension::ExtTextureWebP, "EXT_texture_webp"_s},
            {GltfExtension::KhrMaterialsClearCoat, "KHR_materials_clearcoat"_s},
            {GltfExtension::KhrMaterialsUnlit, "KHR_materials_unlit"_s},
            {GltfExtension::KhrMeshQuantization, "KHR_mesh_quantization"_s},
            {GltfExtension::KhrTextureBasisu, "KHR_texture_basisu"_s},
            {GltfExtension::KhrTextureKtx, "KHR_texture_ktx"_s},
            {GltfExtension::KhrTextureTransform, "KHR_texture_transform"_s},
        };
        for(const Containers::Pair<GltfExtension, Containers::StringView>& i: extensionStrings) {
            if((usedExtensions & i.first()) && !contains(extensionsUsed, i.second()))
                extensionsUsed.push_back(i.second());
            if((_state->requiredExtensions & i.first()) && !contains(extensionsRequired, i.second()))
                extensionsRequired.push_back(i.second());

            usedExtensions &= ~i.first();
        }
        CORRADE_INTERNAL_ASSERT(!usedExtensions);

        if(!extensionsUsed.empty()) {
            json.writeKey("extensionsUsed"_s);
            const Containers::ScopeGuard gltfExtensionsUsed = json.beginArrayScope();
            for(const Containers::StringView i: extensionsUsed) json.write(i);
        }
        if(!extensionsRequired.empty()) {
            json.writeKey("extensionsRequired"_s);
            const Containers::ScopeGuard gltfExtensionsRequired = json.beginArrayScope();
            for(const Containers::StringView i: extensionsRequired) json.write(i);
        }
    }

    /* Wrap up the buffer if it's non-empty or if there are any (empty) buffer
       views referencing it */
    if(!_state->buffer.isEmpty() || !_state->gltfBufferViews.isEmpty()) {
        json.writeKey("buffers"_s);
        const Containers::ScopeGuard gltfBuffers = json.beginArrayScope();
        const Containers::ScopeGuard gltfBuffer = json.beginObjectScope();

        /* If not writing a binary glTF and the buffer is non-empty, save the
           buffer to an external file and reference it. In a binary glTF the
           buffer is just one with an implicit location. */
        if(!_state->binary && !_state->buffer.isEmpty()) {
            if(!_state->filename) {
                Error{} << "Trade::GltfSceneConverter::endData(): can only write a glTF with external buffers if converting to a file";
                return {};
            }

            Containers::String bufferFilename = Utility::Path::splitExtension(*_state->filename).first() + ".bin"_s;
            Utility::Path::write(bufferFilename, _state->buffer);
            /** @todo configurable buffer name? or a path prefix if ending with /?
                or an extension alone if .. what, exactly? */

            /* Writing just the filename as the two files are expected to be
               next to each other */
            json.writeKey("uri"_s).write(Utility::Path::filename(bufferFilename));
        }

        json.writeKey("byteLength"_s).write(_state->buffer.size());
    }

    /* Buffer views, accessors, ... If there are any, the array is left open --
       close it and put the whole JSON into the file */
    if(!_state->gltfBufferViews.isEmpty())
        json.writeKey("bufferViews"_s).writeJson(_state->gltfBufferViews.endArray().toString());
    if(!_state->gltfAccessors.isEmpty())
        json.writeKey("accessors"_s).writeJson(_state->gltfAccessors.endArray().toString());

    /* Write all meshes, first ones that are referenced from a scene and thus
       have a fixed ID, then ones that  */
    if(!_state->meshes.isEmpty()) {
        json.writeKey("meshes"_s);
        const Containers::ScopeGuard gltfMeshes = json.beginArrayScope();

        const auto writeMesh = [](Utility::JsonWriter& json, Containers::ArrayView<const MeshProperties> meshProperties, const Containers::ArrayView<const Containers::Pair<UnsignedInt, Int>> meshMaterialAssignments) {
            const Containers::ScopeGuard gltfMesh = json.beginObjectScope();
            json.writeKey("primitives"_s)
                .beginArray();

            /* One primitive for each mesh/material assignment */
            for(const Containers::Pair<UnsignedInt, Int> meshMaterialAssignment: meshMaterialAssignments) {
                const Containers::ScopeGuard gltfPrimitive = json.beginObjectScope();
                const MeshProperties& mesh = meshProperties[meshMaterialAssignment.first()];

                /* Indices, if any */
                if(mesh.gltfIndices)
                    json.writeKey("indices"_s).write(*mesh.gltfIndices);

                /* Attributes */
                if(!mesh.gltfAttributes.isEmpty()) {
                    json.writeKey("attributes"_s);
                    const Containers::ScopeGuard gltfAttributes = json.beginObjectScope();
                    for(const Containers::Pair<Containers::String, UnsignedInt>& gltfAttribute: mesh.gltfAttributes)
                        json.writeKey(gltfAttribute.first()).write(gltfAttribute.second());
                }

                /* Mode */
                if(mesh.gltfMode)
                    json.writeKey("mode"_s).write(*mesh.gltfMode);

                /* Material */
                if(meshMaterialAssignment.second() != -1)
                    json.writeKey("material"_s).write(meshMaterialAssignment.second());
            }

            json.endArray();

            /* Write a name only if all meshes have the same (which is the case
               for example if the input was parsed by GltfImporter from a
               multi-mesh file) and the name is not empty. This implies that
               the name is also written if there's just a single mesh. */
            Containers::Optional<Containers::StringView> nameToUse;
            for(const Containers::Pair<UnsignedInt, Int> meshMaterialAssignment: meshMaterialAssignments) {
                const Containers::StringView name = meshProperties[meshMaterialAssignment.first()].gltfName;
                if(!nameToUse) {
                    nameToUse = name;
                } else if(nameToUse != name) {
                    nameToUse = {};
                    break;
                }
            }
            if(nameToUse && *nameToUse)
                json.writeKey("name"_s).write(*nameToUse);
        };

        /* Remember which meshes were referenced from a scene. The ones that
           weren't are written at the end, without a material assignment. */
        Containers::BitArray referencedMeshes{DirectInit, _state->meshes.size(), false};

        /* `meshMaterialAssignmentRanges` is either empty or contains i + 1
           items for i meshes referenced by the scene */
        if(!_state->meshMaterialAssignmentRanges.isEmpty()) for(std::size_t i = 0; i != _state->meshMaterialAssignmentRanges.size() - 1; ++i) {
            const Containers::ArrayView<const Containers::Pair<UnsignedInt, Int>> meshMaterialAssignments = _state->meshMaterialAssignments.slice(_state->meshMaterialAssignmentRanges[i], _state->meshMaterialAssignmentRanges[i + 1]);

            for(const Containers::Pair<UnsignedInt, Int> meshMaterialAssignment: meshMaterialAssignments)
                referencedMeshes.set(meshMaterialAssignment.first());

            writeMesh(json, _state->meshes, meshMaterialAssignments);
        }

        for(UnsignedInt i = 0; i != _state->meshes.size(); ++i) {
            if(referencedMeshes[i])
                continue;

            const Containers::Pair<UnsignedInt, Int> meshMaterial[]{{i, -1}};
            writeMesh(json, _state->meshes, meshMaterial);
        }
    }

    if(!_state->gltfMaterials.isEmpty())
        json.writeKey("materials"_s).writeJson(_state->gltfMaterials.endArray().toString());
    if(!_state->gltfSamplers.isEmpty())
        json.writeKey("samplers"_s).writeJson(_state->gltfSamplers.endArray().toString());
    if(!_state->gltfTextures.isEmpty())
        json.writeKey("textures"_s).writeJson(_state->gltfTextures.endArray().toString());
    if(!_state->gltfImages.isEmpty())
        json.writeKey("images"_s).writeJson(_state->gltfImages.endArray().toString());

    /* Nodes and scenes, those got written all at once in
       doAdd(const SceneData&) so no need to close anything */
    if(!_state->gltfNodes.isEmpty())
        json.writeKey("nodes"_s).writeJson(_state->gltfNodes.toString());
    if(!_state->gltfScenes.isEmpty()) {
        json.writeKey("scenes"_s).writeJson(_state->gltfScenes.toString());
        /* Write the default scnee ID, if set. Currently there's at most one
           scene so it can only be either not present or present and set to 0,
           but certain importers might require it to be present. */
        if(_state->defaultScene != -1)
            json.writeKey("scene"_s).write(_state->defaultScene);
    }

    /* Done! */
    json.endObject();

    union CharCaster {
        UnsignedInt value;
        const char data[4];
    };

    /* Padding after JSON and binary chunks to satisfy four-byte alignment
       requirements. Used only in case of a binary output. */
    std::size_t jsonChunkPadding;
    std::size_t binChunkPadding;

    /* Reserve the output array and write headers for a binary glTF */
    Containers::Array<char> out;
    if(_state->binary) {
        jsonChunkPadding = 4*((json.size() + 3)/4) - json.size();
        binChunkPadding = 4*((_state->buffer.size() + 3)/4) - _state->buffer.size();
        CORRADE_INTERNAL_ASSERT(jsonChunkPadding <= 3 && binChunkPadding <= 3);

        const std::size_t totalSize = 12 + /* file header */
            /* JSON chunk + header + padding */
            8 + json.size() + jsonChunkPadding +
            /* BIN chunk + header + padding */
            (_state->buffer.isEmpty() ? 0 :
                8 + _state->buffer.size() + binChunkPadding);
        Containers::arrayReserve<ArrayAllocator>(out, totalSize);

        /* glTF header */
        Containers::arrayAppend<ArrayAllocator>(out,
            "glTF\x02\x00\x00\x00"_s);
        Containers::arrayAppend<ArrayAllocator>(out,
            CharCaster{UnsignedInt(totalSize)}.data);

        /* JSON chunk header. The size includes padding. */
        Containers::arrayAppend<ArrayAllocator>(out,
            CharCaster{UnsignedInt(json.size() + jsonChunkPadding)}.data);
        Containers::arrayAppend<ArrayAllocator>(out,
            "JSON"_s);

    /* Otherwise reserve just for the JSON. Set the padding values to a
       nonsense to catch accidental uses. */
    } else {
        Containers::arrayReserve<ArrayAllocator>(out, json.size());
        jsonChunkPadding = binChunkPadding = ~std::size_t{};
    }

    /* Copy the JSON data to the output. In case of a text glTF we would
       ideally just pass the memory from the JsonWriter but the class uses an
       arbitrary growable deleter internally and custom deleters are forbidden
       in plugins. */
    /** @todo make it possible to specify an external allocator in JsonWriter
        once allocators-as-arguments are a thing */
    Containers::arrayAppend<ArrayAllocator>(out, json.toString());

    /* In case of a binary glTF add more data after */
    if(_state->binary) {
        /* JSON chunk padding, have to be spaces */
        for(char& i: Containers::arrayAppend<ArrayAllocator>(out, NoInit, jsonChunkPadding))
            i = ' ';

        /* Add the buffer as a second BIN chunk. The size includes padding
           again, this time the padding has to be zeros. */
        if(!_state->buffer.isEmpty()) {
            Containers::arrayAppend<ArrayAllocator>(out,
                CharCaster{UnsignedInt(_state->buffer.size() + binChunkPadding)}.data);
            Containers::arrayAppend<ArrayAllocator>(out,
                "BIN\0"_s);
            Containers::arrayAppend<ArrayAllocator>(out,
                _state->buffer);
            for(char& i: Containers::arrayAppend<ArrayAllocator>(out, NoInit, binChunkPadding))
                i = '\0';
        }
    }

    /* GCC 4.8 and Clang 3.8 need extra help here */
    return Containers::optional(Utility::move(out));
}

void GltfSceneConverter::doAbort() {
    _state = {};
}

void GltfSceneConverter::doSetDefaultScene(const UnsignedInt id) {
    _state->defaultScene = id;
}

void GltfSceneConverter::doSetObjectName(const UnsignedLong object, const Containers::StringView name) {
    if(_state->objectNames.size() <= object)
        arrayResize(_state->objectNames, object + 1);
    _state->objectNames[object] = Containers::String::nullTerminatedGlobalView(name);
}

void GltfSceneConverter::doSetSceneFieldName(const SceneField field, const Containers::StringView name) {
    _state->sceneFieldNames[sceneFieldCustom(field)] = Containers::String::nullTerminatedGlobalView(name);
}

bool GltfSceneConverter::doAdd(const UnsignedInt id, const SceneData& scene, const Containers::StringView name) {
    if(!scene.is3D()) {
        Error{} << "Trade::GltfSceneConverter::add(): expected a 3D scene";
        return {};
    }

    /** @todo multi-scene support could be done by remembering object IDs used
        by the scenes, and then:
        -   if the same objects are referenced from another scene, only using
            the Parent field from them (which would be different in order to
            make the same nodes appear in (different subtrees of) different
            scenes), and assuming everything else would be the same
        -   if new objects are referenced from it, add them as completely fresh
            (the IDs aren't preserved anyway, so it's no problem if they're
            added at the end) */
    if(id > 0) {
        Error{} << "Trade::GltfSceneConverter::add(): only one scene is supported at the moment";
        return {};
    }

    const Containers::Optional<UnsignedInt> parentFieldId = scene.findFieldId(SceneField::Parent);
    const std::size_t parentFieldSize = parentFieldId ? scene.fieldSize(*parentFieldId) : 0;

    /* Temporary storage for scene hierarchy processing */
    Containers::ArrayView<UnsignedInt> mappingStorage;
    Containers::ArrayView<UnsignedInt> outputMapping;
    Containers::ArrayView<Int> parents;
    Containers::ArrayView<Int> parentsExpanded;
    Containers::ArrayView<UnsignedInt> children;
    Containers::ArrayView<UnsignedInt> childOffsets;
    Containers::ArrayView<std::size_t> objectFieldOffsets;
    Containers::MutableBitArrayView hasData;
    Containers::MutableBitArrayView hasParent;
    Containers::ArrayTuple storage{
        {NoInit, scene.fieldSizeBound(), mappingStorage},
        {NoInit, std::size_t(scene.mappingBound()), outputMapping},
        {NoInit, parentFieldSize, parents},
        {NoInit, std::size_t(scene.mappingBound()), parentsExpanded},
        {NoInit, parentFieldSize, children},
        /* The first element is 0, the second is root object count */
        {ValueInit, std::size_t(scene.mappingBound() + 2), childOffsets},
        {ValueInit, std::size_t(scene.mappingBound() + 2), objectFieldOffsets},
        {ValueInit, std::size_t(scene.mappingBound()), hasData},
        {ValueInit, std::size_t(scene.mappingBound()), hasParent}
    };

    /* Convert parent pointers to a child list, verify sanity of the
       hierarchy */
    if(parentFieldSize) {
        const Containers::ArrayView<UnsignedInt> parentMapping = mappingStorage.prefix(parentFieldSize);
        scene.parentsInto(parentMapping, parents);

        /* Create a mask containing only objects that have a parent field */
        for(const UnsignedInt object: parentMapping) {
            if(object >= scene.mappingBound()) {
                Error{} << "Trade::GltfSceneConverter::add(): scene parent mapping" << object << "out of range for" << scene.mappingBound() << "objects";
                return {};
            }

            if(hasParent[object]) {
                Error{} << "Trade::GltfSceneConverter::add(): object" << object << "has more than one parent";
                return {};
            }

            hasParent.set(object);
        }

        /* Find cycles, Tortoise and Hare. Needs to have the parents field
           expanded to be addressable in O(1). */
        {
            for(std::size_t i = 0; i != parentsExpanded.size(); ++i)
                parentsExpanded[i] = -1;
            for(std::size_t i = 0; i != parentMapping.size(); ++i) {
                const Int parent = parents[i];
                if(parent != -1 && UnsignedInt(parent) >= scene.mappingBound()) {
                    Error{} << "Trade::GltfSceneConverter::add(): scene parent reference" << parent << "out of range for" << scene.mappingBound() << "objects";
                    return {};
                }

                parentsExpanded[parentMapping[i]] = parent;
            }

            for(std::size_t i = 0; i != parentsExpanded.size(); ++i) {
                Int p1 = parentsExpanded[i];
                Int p2 = p1 < 0 ? -1 : parentsExpanded[p1];

                while(p1 >= 0 && p2 >= 0) {
                    if(p1 == p2) {
                        Error{} << "Trade::GltfSceneConverter::add(): scene hierarchy contains a cycle starting at object" << i;
                        return {};
                    }

                    p1 = parentsExpanded[p1];
                    p2 = parentsExpanded[p2] < 0 ? -1 : parentsExpanded[parentsExpanded[p2]];
                }
            }
        }

        /* Create a contiguous mapping for only objects with a parent field */
        UnsignedInt outputMappingOffset = 0;
        for(std::size_t i = 0; i != scene.mappingBound(); ++i) {
            if(!hasParent[i])
                continue;
            outputMapping[i] = outputMappingOffset++;
        }

        /* Calculate count of children for every object. Initially shifted
           by two values, `childOffsets[i + 2]` is the count of children for
           object `i`, `childOffsets[1]` is the count of root objects,
           `childOffsets[0]` is 0. */
        for(const Int parent: parents)
            ++childOffsets[parent + 2];

        /** @todo detect nodes that have a parent but the parent itself has
            no parent, i.e. loose subtrees, and either ignore or warn about
            them? Or make that an officially supported feature that allows
            writing loose nodes to the file? */

        /* Turn that into an offset array. This makes it shifted by just one
           value, so `childOffsets[i + 2] - childOffsets[i + 1]` is the count
           of children for object `i`; `childOffsets[1]` is the count of
           root objects, `childOffsets[0]` is 0. */
        std::size_t offset = 0;
        for(UnsignedInt& i: childOffsets) {
            const std::size_t count = i;
            i += offset;
            offset += count;
        }
        CORRADE_INTERNAL_ASSERT(offset == parents.size());

        /* Populate the child array. This makes `childOffsets` finally
           unshifted, so `children[childOffsets[i]]` to
           `children[childOffsets[i + 1]]` contains children of object `i`;
           `children[0]` until `childOffsets[i]` contains root objects. */
        for(std::size_t i = 0; i != parentMapping.size(); ++i) {
            const UnsignedInt object = parentMapping[i];

            children[childOffsets[parents[i] + 1]++] = outputMapping[object];
        }
        CORRADE_INTERNAL_ASSERT(
            childOffsets[childOffsets.size() - 1] == parentFieldSize &&
            childOffsets[childOffsets.size() - 2] == parentFieldSize);
    }

    /* A mask for skipping fields that were deliberately left out due to being
       handled differently, having unsupported formats etc. */
    Containers::BitArray usedFields{ValueInit, scene.fieldCount()};

    /* Calculate count of field assignments for each object. Initially shifted
       by two values, `objectFieldOffsets[i + 2]` is the count of fields for
       object `i`. */
    for(UnsignedInt i = 0, iMax = scene.fieldCount(); i != iMax; ++i) {
        const SceneField fieldName = scene.fieldName(i);

        /* Skip fields that are treated differently */
        if(
            /* Parents are converted to a child list instead -- a presence of
               a parent field doesn't say anything about given object having
               any children */
            fieldName == SceneField::Parent ||
            /* Materials are tied to the Mesh field -- if Mesh exists,
               Materials have the exact same mapping, thus there's no point in
               counting them separately */
            fieldName == SceneField::MeshMaterial
        )
            continue;

        /* Custom fields */
        if(isSceneFieldCustom(fieldName)) {
            /* Skip ones for which we don't have a name */
            const auto found = _state->sceneFieldNames.find(sceneFieldCustom(fieldName));
            if(found == _state->sceneFieldNames.end()) {
                if(!(flags() & SceneConverterFlag::Quiet))
                    Warning{} << "Trade::GltfSceneConverter::add(): custom scene field" << sceneFieldCustom(fieldName) << "has no name assigned, skipping";
                continue;
            }

            /* Allow only scalar numbers for now */
            /** @todo For vectors / matrices it would be about `+= size`
                instead of `++objectFieldOffsets` below */
            const SceneFieldType type = scene.fieldType(i);
            if(type != SceneFieldType::UnsignedInt &&
               type != SceneFieldType::Int &&
               type != SceneFieldType::Float &&
               type != SceneFieldType::Bit &&
               !Implementation::isSceneFieldTypeString(type))
            {
                if(!(flags() & SceneConverterFlag::Quiet))
                    Warning{} << "Trade::GltfSceneConverter::add(): custom scene field" << found->second << "has unsupported type" << type << Debug::nospace << ", skipping";
                continue;
            }
        }

        usedFields.set(i);

        const Containers::ArrayView<UnsignedInt> mapping = mappingStorage.prefix(scene.fieldSize(i));
        scene.mappingInto(i, mapping);
        for(const UnsignedInt object: mapping) {
            if(object >= scene.mappingBound()) {
                Error{} << "Trade::GltfSceneConverter::add():" << scene.fieldName(i) << "mapping" << object << "out of range for" << scene.mappingBound() << "objects";
                return {};
            }

            /* Mark that the object has data. Will be used later to warn about
               objects that contained data but had no parents and thus were
               unused. */
            hasData.set(object);

            /* Objects that have no parent field are not exported thus their
               fields don't need to be counted either */
            if(!hasParent[object])
                continue;

            ++objectFieldOffsets[object + 2];
        }
    }

    /* Turn that into an offset array. This makes it shifted by just one value,
       so `objectFieldOffsets[i + 2] - objectFieldOffsets[i + 1]` is the count
       of fields for object `i`. */
    std::size_t totalFieldCount = 0;
    for(std::size_t& i: objectFieldOffsets) {
        const std::size_t count = i;
        i += totalFieldCount;
        totalFieldCount += count;
    }

    /* Retrieve sizes of exported fields, print a warning for unused ones */
    std::size_t transformationCount = 0;
    std::size_t trsCount = 0;
    bool hasTranslation = false;
    bool hasRotation = false;
    bool hasScaling = false;
    std::size_t meshMaterialCount = 0;
    std::size_t customFieldCount = 0;
    std::size_t customBitFieldCount = 0;
    std::size_t customStringFieldCount = 0;
    for(UnsignedInt i = 0; i != scene.fieldCount(); ++i) {
        if(!usedFields[i])
            continue;

        const std::size_t size = scene.fieldSize(i);
        const SceneField fieldName = scene.fieldName(i);
        switch(fieldName) {
            case SceneField::Transformation:
                transformationCount = size;
                continue;
            case SceneField::Translation:
                hasTranslation = true;
                trsCount = size;
                continue;
            case SceneField::Rotation:
                hasRotation = true;
                trsCount = size;
                continue;
            case SceneField::Scaling:
                hasScaling = true;
                trsCount = size;
                continue;
            case SceneField::Mesh:
                meshMaterialCount = size;
                continue;

            /* ImporterState is ignored without a warning, it makes no sense to
               save a pointer value */
            case SceneField::ImporterState:
                continue;

            /* The add() API is not implemented for these yet, thus any
               reference of them would cause an OOB in the resulting glTF */
            case SceneField::Light:
            case SceneField::Camera:
            case SceneField::Skin:
                break;

            /* These should be excluded from the usedFields mask already */
            /* LCOV_EXCL_START */
            case SceneField::Parent:
            case SceneField::MeshMaterial:
                CORRADE_INTERNAL_ASSERT_UNREACHABLE();
            /* LCOV_EXCL_STOP */
        }

        if(isSceneFieldCustom(fieldName)) {
            const SceneFieldType fieldType = scene.fieldType(i);
            if(fieldType == SceneFieldType::Bit)
                customBitFieldCount += size;
            else if(Implementation::isSceneFieldTypeString(fieldType))
                customStringFieldCount += size;
            else
                customFieldCount += size;
        }
        else if(!(flags() & SceneConverterFlag::Quiet))
            Warning{} << "Trade::GltfSceneConverter::add():" << scene.fieldName(i) << "was not used";
    }

    /* Allocate space for field IDs and offsets as well as actual field data.
       If there are objects without parents, some suffix in the field data
       arrays will stay unused. */
    Containers::ArrayView<UnsignedInt> fieldIds;
    Containers::ArrayView<std::size_t> fieldOffsets;
    Containers::ArrayView<Matrix4> transformations;
    Containers::ArrayView<Vector3> translations;
    Containers::ArrayView<Quaternion> rotations;
    Containers::ArrayView<Vector3> scalings;
    Containers::StridedArrayView1D<Containers::Pair<UnsignedInt, Int>> meshesMaterials;
    Containers::MutableBitArrayView hasTrs;
    /** @todo Abusing the fact that all allowed extras types are 32-bit now,
        when 64-bit types are introduced there has to be a second 64-bit array
        to satisfy alignment. For composite types (vectors, matrices) however
        it's enough to just take more items at once. */
    Containers::ArrayView<UnsignedInt> customFieldsUnsignedInt;
    Containers::MutableBitArrayView customFieldsBit;
    Containers::ArrayView<Containers::StringView> customFieldsString;
    Containers::ArrayTuple fieldStorage{
        {NoInit, totalFieldCount, fieldIds},
        {NoInit, totalFieldCount, fieldOffsets},
        {NoInit, transformationCount, transformations},
        {NoInit, hasTranslation ? trsCount : 0, translations},
        {NoInit, hasRotation ? trsCount : 0, rotations},
        {NoInit, hasScaling ? trsCount : 0, scalings},
        {NoInit, meshMaterialCount, meshesMaterials},
        {ValueInit, std::size_t(scene.mappingBound()), hasTrs},
        {NoInit, customFieldCount, customFieldsUnsignedInt},
        {NoInit, customBitFieldCount, customFieldsBit},
        {NoInit, customStringFieldCount, customFieldsString},
    };
    const auto customFieldsFloat = Containers::arrayCast<Float>(customFieldsUnsignedInt);
    const auto customFieldsInt = Containers::arrayCast<Int>(customFieldsUnsignedInt);

    /* Populate field ID and offset arrays. This makes `objectFieldOffsets`
       finally unshifted, so `fieldIds[objectFieldOffsets[i]]` to
       `fieldIds[objectFieldOffsets[i + 1]]` contains field IDs for object `i`,
       same with `offsets`. */
    for(UnsignedInt i = 0, iMax = scene.fieldCount(); i != iMax; ++i) {
        /* Custom fields are handled in a separate loop below */
        if(!usedFields[i] || isSceneFieldCustom(scene.fieldName(i)))
            continue;

        const std::size_t fieldSize = scene.fieldSize(i);
        const Containers::ArrayView<UnsignedInt> mapping = mappingStorage.prefix(fieldSize);
        scene.mappingInto(i, mapping);
        for(std::size_t j = 0; j != fieldSize; ++j) {
            const UnsignedInt object = mapping[j];

            /* Objects that have no parent field are not exported thus their
               fields don't need to be counted either */
            if(!hasParent[object])
                continue;

            std::size_t& objectFieldOffset = objectFieldOffsets[object + 1];
            fieldIds[objectFieldOffset] = i;
            fieldOffsets[objectFieldOffset] = j;
            ++objectFieldOffset;
        }
    } {
        std::size_t offset = 0;
        std::size_t bitOffset = 0;
        std::size_t stringOffset = 0;
        for(UnsignedInt i = 0, iMax = scene.fieldCount(); i != iMax; ++i) {
            /* Only custom fields here, this means they're always last and all
               together, which makes it possible to write the "extras" object
               in one run. */
            if(!usedFields[i] || !isSceneFieldCustom(scene.fieldName(i)))
                continue;

            const SceneFieldType fieldType = scene.fieldType(i);
            std::size_t* usedOffset;
            if(fieldType == SceneFieldType::Bit)
                usedOffset = &bitOffset;
            else if(Implementation::isSceneFieldTypeString(fieldType))
                usedOffset = &stringOffset;
            else
                usedOffset = &offset;

            const std::size_t fieldSize = scene.fieldSize(i);
            const Containers::ArrayView<UnsignedInt> mapping = mappingStorage.prefix(fieldSize);
            scene.mappingInto(i, mapping);
            for(std::size_t j = 0; j != fieldSize; ++j) {
                const UnsignedInt object = mapping[j];

                /* Objects that have no parent field are not exported thus
                   their fields don't need to be counted either */
                if(!hasParent[object])
                    continue;

                std::size_t& objectFieldOffset = objectFieldOffsets[object + 1];
                fieldIds[objectFieldOffset] = i;
                /* As we put all custom fields into a single array, the offset
                   needs to also include sizes of all previous custom fields
                   already written. */
                fieldOffsets[objectFieldOffset] = *usedOffset + j;
                ++objectFieldOffset;
            }

            *usedOffset += fieldSize;
        }
    }
    CORRADE_INTERNAL_ASSERT(
        objectFieldOffsets[0] == 0 &&
        objectFieldOffsets[objectFieldOffsets.size() - 1] == totalFieldCount &&
        objectFieldOffsets[objectFieldOffsets.size() - 2] == totalFieldCount);

    /* Populate field data, check their bounds */
    if(transformationCount)
        scene.transformations3DInto(nullptr, transformations);
    if(trsCount) {
        /* Objects that have TRS will have the matrix omitted */
        const Containers::ArrayView<UnsignedInt> mapping = mappingStorage.prefix(trsCount);
        scene.translationsRotationsScalings3DInto(mapping,
            hasTranslation ? translations : nullptr,
            hasRotation ? rotations : nullptr,
            hasScaling ? scalings : nullptr);
        for(const UnsignedInt i: mapping)
            hasTrs.set(i);
    }
    if(meshMaterialCount) {
        scene.meshesMaterialsInto(nullptr,
            meshesMaterials.slice(&decltype(meshesMaterials)::Type::first),
            meshesMaterials.slice(&decltype(meshesMaterials)::Type::second));
        for(const UnsignedInt mesh: meshesMaterials.slice(&decltype(meshesMaterials)::Type::first)) {
            if(mesh >= meshCount()) {
                Error{} << "Trade::GltfSceneConverter::add(): scene references mesh" << mesh << "but only" << meshCount() << "were added so far";
                return {};
            }
        }
        for(const Int material: meshesMaterials.slice(&decltype(meshesMaterials)::Type::second)) {
            if(material != -1 && UnsignedInt(material) >= materialCount()) {
                Error{} << "Trade::GltfSceneConverter::add(): scene references material" << material << "but only" << materialCount() << "were added so far";
                return {};
            }
        }
    }

    /* Populate custom field data */
    {
        std::size_t offset = 0;
        std::size_t bitOffset = 0;
        std::size_t stringOffset = 0;
        for(std::size_t i = 0; i != scene.fieldCount(); ++i) {
            if(!usedFields[i] || !isSceneFieldCustom(scene.fieldName(i)))
                continue;

            /** @todo this could be easily extended for 8- and 16-bit values,
                just Math::cast()'ing them to the output */
            const SceneFieldType type = scene.fieldType(i);
            const std::size_t size = scene.fieldSize(i);
            if(type == SceneFieldType::UnsignedInt) {
                Utility::copy(scene.field<UnsignedInt>(i),
                    customFieldsUnsignedInt.slice(offset, offset + size));
                offset += size;
            } else if(type == SceneFieldType::Int) {
                Utility::copy(scene.field<Int>(i),
                    customFieldsInt.slice(offset, offset + size));
                offset += size;
            } else if(type == SceneFieldType::Float) {
                Utility::copy(scene.field<Float>(i),
                    customFieldsFloat.slice(offset, offset + size));
                offset += size;
            } else if(type == SceneFieldType::Bit) {
                /** @todo Utility::copy() for bits, *finally* */
                const Containers::StridedBitArrayView1D bits = scene.fieldBits(i);
                for(std::size_t j = 0; j != bits.size(); ++j)
                    customFieldsBit.set(bitOffset + j, bits[j]);
                bitOffset += size;
            } else if(Implementation::isSceneFieldTypeString(type)) {
                const Containers::StringIterable strings = scene.fieldStrings(i);
                for(std::size_t j = 0; j != strings.size(); ++j)
                    customFieldsString[stringOffset + j] = strings[j];
                stringOffset += size;
            } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }

        CORRADE_INTERNAL_ASSERT(
            offset == customFieldCount &&
            bitOffset == customBitFieldCount &&
            stringOffset == customStringFieldCount);
    }

    /* Go object by object and consume the fields, populating the glTF node
       array. The output is currently restricted to a single scene, so the
       glTF nodes array should still be empty at this point. Otherwise we'd
       have broken child node indexing. */
    CORRADE_INTERNAL_ASSERT(_state->gltfNodes.isEmpty());
    /* Delaying opening of the node array until there's an actual node to be
       written -- it could be that there's no nodes at all or that none of them
       has a parent, in which case the nodes array doesn't need to be written
       at all. */
    Containers::ScopeGuard gltfNodes{NoCreate};
    for(UnsignedLong object = 0; object != scene.mappingBound(); ++object) {
        /* Objects that have no parent field are not exported */
        if(!hasParent[object]) {
            if(!(flags() & SceneConverterFlag::Quiet) && hasData[object])
                Warning{} << "Trade::GltfSceneConverter::add(): parentless object" << object << "was not used";
            continue;
        }

        if(_state->gltfNodes.isEmpty())
            gltfNodes = _state->gltfNodes.beginArrayScope();
        const Containers::ScopeGuard gltfNode = _state->gltfNodes.beginObjectScope();

        /* Write the children array, if there's any */
        if(childOffsets[object + 1] - childOffsets[object]) {
            _state->gltfNodes.writeKey("children"_s).writeArray(children.slice(childOffsets[object], childOffsets[object + 1]));
        }

        /* Whether glTF node extras object for custom fields is open. This
           should always happen only after all non-custom fields are written
           (to avoid unrelated data being written inside extras as well), and
           is checked below. */
        bool extrasOpen = false;

        /* A temporary array for sorting multi-mesh assignments in order to
           deduplicate them. Gets cleared and refilled for each object that has
           a Mesh field. */
        Containers::Array<Containers::Pair<UnsignedInt, Int>> sortedMeshMaterialAssignments;

        SceneField previous{};
        for(std::size_t i = objectFieldOffsets[object], iMax = objectFieldOffsets[object + 1]; i != iMax; ++i) {
            const std::size_t offset = fieldOffsets[i];
            const SceneField fieldName = scene.fieldName(fieldIds[i]);
            const SceneFieldFlags fieldFlags = scene.fieldFlags(fieldIds[i]);

            /* If the field is a mesh assignment or is custom and marked as
               MultiEntry, grab all values for this object (they're stored
               consecutively in that case) */
            std::size_t multiEntryFieldSize = 0;
            if(fieldName == SceneField::Mesh ||
               (isSceneFieldCustom(fieldName) && (fieldFlags & SceneFieldFlag::MultiEntry)))
            {
                do ++multiEntryFieldSize;
                while(i + multiEntryFieldSize != iMax && fieldIds[i + multiEntryFieldSize] == fieldIds[i]);

            /* Warn otherwise -- for custom fields that have duplicates but
               aren't marked with MultiEntry, and for builtin fields as well */
            } else if(fieldName == previous) {
                if(!(flags() & SceneConverterFlag::Quiet)) {
                    Warning w;
                    w << "Trade::GltfSceneConverter::add(): ignoring duplicate field";
                    if(isSceneFieldCustom(fieldName)) {
                        const auto found = _state->sceneFieldNames.find(sceneFieldCustom(fieldName));
                        CORRADE_INTERNAL_ASSERT(found != _state->sceneFieldNames.end());
                        w << found->second;
                    } else w << previous;
                    w << "for object" << object;
                }
                continue;
            }

            previous = fieldName;

            /* If the field is custom, handle it and continue to the next
               one (which should also be a custom one, if there's any). If
               it's not custom, the extras object should not be open. */
            if(isSceneFieldCustom(fieldName)) {
                if(!extrasOpen) {
                    _state->gltfNodes.writeKey("extras"_s).beginObject();
                    extrasOpen = true;
                }

                const auto found = _state->sceneFieldNames.find(sceneFieldCustom(fieldName));
                CORRADE_INTERNAL_ASSERT(found != _state->sceneFieldNames.end());
                _state->gltfNodes.writeKey(found->second);

                /* If there's an array, write it as such */
                const SceneFieldType type = scene.fieldType(fieldIds[i]);
                if(multiEntryFieldSize) {
                    _state->gltfNodes.beginCompactArray();
                    if(type == SceneFieldType::UnsignedInt) {
                        for(std::size_t j = 0; j != multiEntryFieldSize; ++j)
                            _state->gltfNodes.write(customFieldsUnsignedInt[fieldOffsets[i + j]]);
                    } else if(type == SceneFieldType::Int) {
                        for(std::size_t j = 0; j != multiEntryFieldSize; ++j)
                            _state->gltfNodes.write(customFieldsInt[fieldOffsets[i + j]]);
                    } else if(type == SceneFieldType::Float) {
                        for(std::size_t j = 0; j != multiEntryFieldSize; ++j)
                            _state->gltfNodes.write(customFieldsFloat[fieldOffsets[i + j]]);
                    } else if(type == SceneFieldType::Bit) {
                        for(std::size_t j = 0; j != multiEntryFieldSize; ++j)
                            _state->gltfNodes.write(customFieldsBit[fieldOffsets[i + j]]);
                    } else if(Implementation::isSceneFieldTypeString(type)) {
                        for(std::size_t j = 0; j != multiEntryFieldSize; ++j)
                            _state->gltfNodes.write(customFieldsString[fieldOffsets[i + j]]);
                    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
                    _state->gltfNodes.endArray();

                    /* Skip the remaining array items as otherwise they would
                       warn in the next iteration */
                    i += multiEntryFieldSize - 1;

                /* Otherwise write a scalar */
                } else {
                    if(type == SceneFieldType::UnsignedInt)
                        _state->gltfNodes.write(customFieldsUnsignedInt[offset]);
                    else if(type == SceneFieldType::Int)
                        _state->gltfNodes.write(customFieldsInt[offset]);
                    else if(type == SceneFieldType::Float)
                        _state->gltfNodes.write(customFieldsFloat[offset]);
                    else if(type == SceneFieldType::Bit)
                        _state->gltfNodes.write(customFieldsBit[offset]);
                    else if(Implementation::isSceneFieldTypeString(type))
                        _state->gltfNodes.write(customFieldsString[offset]);
                    else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
                }

                continue;
            } else CORRADE_INTERNAL_ASSERT(!extrasOpen);

            if(fieldName == SceneField::Transformation) {
                /* § 5.25 (Node) says a node can have either a matrix or a TRS,
                   which doesn't really make it clear if both are allowed. But
                   further down it says "When a node is targeted for animation (referenced by an animation.channel.target), matrix MUST NOT
                   be present." so I guess it's an exclusive or, thus a matrix
                   gets written only if there's no TRS. */
                if(transformations[offset] != Matrix4{} && !hasTrs[object])
                    _state->gltfNodes.writeKey("matrix"_s).writeArray(transformations[offset].data(), 4);
            } else if(fieldName == SceneField::Translation) {
                if(translations[offset] != Vector3{})
                    _state->gltfNodes.writeKey("translation"_s).writeArray(translations[offset].data());
            } else if(fieldName == SceneField::Rotation) {
                if(rotations[offset] != Quaternion{})
                    /* glTF also uses the XYZW order */
                    _state->gltfNodes.writeKey("rotation"_s).writeArray(rotations[offset].data());
            } else if(fieldName == SceneField::Scaling) {
                if(scalings[offset] != Vector3{1.0f})
                    _state->gltfNodes.writeKey("scale"_s).writeArray(scalings[offset].data());
            } else if(fieldName == SceneField::Mesh) {
                /* Gather and sort all mesh assignments */
                CORRADE_INTERNAL_ASSERT(multiEntryFieldSize >= 1);
                /** @todo arrayClear(), *finally */
                arrayResize(sortedMeshMaterialAssignments, 0);
                for(std::size_t j = 0; j != multiEntryFieldSize; ++j)
                    arrayAppend(sortedMeshMaterialAssignments, meshesMaterials[fieldOffsets[i + j]]);
                std::sort(sortedMeshMaterialAssignments.begin(), sortedMeshMaterialAssignments.end(), [](const Containers::Pair<UnsignedInt, Int> a, const Containers::Pair<UnsignedInt, Int> b) {
                    return
                        a.first() < b.first() ||
                        (a.first() == b.first() && a.second() < b.second());
                });

                /* Try to find this sequence among already used assignments.
                   `meshMaterialAssignmentRanges` is either empty or contains
                   i + 1 items for i meshes referenced by the scene. */
                Containers::Optional<UnsignedInt> meshId;
                if(!_state->meshMaterialAssignmentRanges.isEmpty()) for(UnsignedInt j = 0; j != _state->meshMaterialAssignmentRanges.size() - 1; ++j) {
                    const Containers::ArrayView<const Containers::Pair<UnsignedInt, Int>> meshMaterialAssignmentCandidates = _state->meshMaterialAssignments.slice(_state->meshMaterialAssignmentRanges[j], _state->meshMaterialAssignmentRanges[j + 1]);

                    /* If it has different mesh count, it's definitely not what
                       we're looking for */
                    if(meshMaterialAssignmentCandidates.size() != sortedMeshMaterialAssignments.size())
                        continue;

                    /* Inverse the logic to not need to invent horrible shit to
                       break out of two loops at once -- set the meshId upfront
                       and unset it if comparison fails, then break out of the
                       outer loop if it's still set at the end */
                    meshId = j;
                    /** @todo something better than O(n^3) lookup? sigh */
                    for(std::size_t k = 0; k != sortedMeshMaterialAssignments.size(); ++k) {
                        if(meshMaterialAssignmentCandidates[k] != sortedMeshMaterialAssignments[k]) {
                            meshId = {};
                            break;
                        }
                    }
                    if(meshId)
                        break;
                }

                /* If not found, add the sorted sequence as a new assignment.
                   If meshMaterialAssignmentRanges is empty, add 0 as the first
                   element. */
                if(!meshId) {
                    arrayAppend(_state->meshMaterialAssignments, sortedMeshMaterialAssignments);

                    if(_state->meshMaterialAssignmentRanges.isEmpty())
                        arrayAppend(_state->meshMaterialAssignmentRanges, 0);

                    meshId = _state->meshMaterialAssignmentRanges.size() - 1;
                    arrayAppend(_state->meshMaterialAssignmentRanges, _state->meshMaterialAssignments.size());
                }

                _state->gltfNodes.writeKey("mesh"_s).write(*meshId);

                /* Skip the remaining mesh assignments as otherwise they would
                   warn in the next iteration */
                i += multiEntryFieldSize - 1;

            } else if(fieldName == SceneField::Parent ||
                      fieldName == SceneField::MeshMaterial) {
                /* Skipped when counting the fields, thus shouldn't appear
                   here */
                CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

            /* Not-yet-handled field, nothing to do. Doesn't make sense to
               filter them in the field ID/offset-populating loop above as most
               fields including custom ones will be eventually supported
               anyway. */
            } else continue;
        }

        if(extrasOpen)
            _state->gltfNodes.endObject();

        if(_state->objectNames.size() > object && _state->objectNames[object])
            _state->gltfNodes.writeKey("name"_s).write(_state->objectNames[object]);
    }

    /* Scene object referencing the root children */
    CORRADE_INTERNAL_ASSERT(_state->gltfScenes.isEmpty());
    const Containers::ScopeGuard gltfScenes = _state->gltfScenes.beginArrayScope();
    CORRADE_INTERNAL_ASSERT(_state->gltfScenes.currentArraySize() == id);
    const Containers::ScopeGuard gltfScene = _state->gltfScenes.beginObjectScope();
    if(childOffsets[0]) {
        _state->gltfScenes.writeKey("nodes"_s).writeArray(children.prefix(childOffsets[0]));
    }

    if(name)
        _state->gltfScenes.writeKey("name"_s).write(name);

    return true;
}

void GltfSceneConverter::doSetMeshAttributeName(const MeshAttribute attribute, Containers::StringView name) {
    /* Replace the previous entry if already set */
    for(Containers::Pair<MeshAttribute, Containers::String>& i: _state->customMeshAttributes) {
        if(i.first() == attribute) {
            i.second() = Containers::String::nullTerminatedGlobalView(name);
            return;
        }
    }

    arrayAppend(_state->customMeshAttributes, InPlaceInit, attribute, Containers::String::nullTerminatedGlobalView(name));
}

bool GltfSceneConverter::doAdd(const UnsignedInt id, const MeshData& mesh, const Containers::StringView name) {
    /* Check and convert mesh primitive */
    /** @todo check primitive count according to the spec */
    Int gltfMode;
    switch(mesh.primitive()) {
        case MeshPrimitive::Points:
            gltfMode = Implementation::GltfModePoints;
            break;
        case MeshPrimitive::Lines:
            gltfMode = Implementation::GltfModeLines;
            break;
        case MeshPrimitive::LineLoop:
            gltfMode = Implementation::GltfModeLineLoop;
            break;
        case MeshPrimitive::LineStrip:
            gltfMode = Implementation::GltfModeLineStrip;
            break;
        case MeshPrimitive::Triangles:
            gltfMode = Implementation::GltfModeTriangles;
            break;
        case MeshPrimitive::TriangleStrip:
            gltfMode = Implementation::GltfModeTriangleStrip;
            break;
        case MeshPrimitive::TriangleFan:
            gltfMode = Implementation::GltfModeTriangleFan;
            break;
        default:
            Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh primitive" << mesh.primitive();
            return {};
    }

    /* Check and convert mesh index type. Have to zero-initialize even though
       it's *clearly* set in all cases below because otherwise GCC 11, 12, 13
       in Release complains that "warning: ‘gltfIndexType’ may be used
       uninitialized". Unhelpful time-wasting warnings yet again.

       MSVC warns too, but that one I forgive. Can't expect much from such a
       pile of poo. */
    #if (defined(CORRADE_TARGET_GCC) && !defined(CORRADE_TARGET_CLANG) && __GNUC__ >= 11) || defined(CORRADE_TARGET_MSVC)
    Int gltfIndexType{};
    #else
    Int gltfIndexType;
    #endif
    if(mesh.isIndexed()) {
        if(!mesh.indices().isContiguous()) {
            Error{} << "Trade::GltfSceneConverter::add(): non-contiguous mesh index arrays are not supported";
            return {};
        }
        switch(mesh.indexType()) {
            case MeshIndexType::UnsignedByte:
                gltfIndexType = Implementation::GltfTypeUnsignedByte;
                break;
            case MeshIndexType::UnsignedShort:
                gltfIndexType = Implementation::GltfTypeUnsignedShort;
                break;
            case MeshIndexType::UnsignedInt:
                gltfIndexType = Implementation::GltfTypeUnsignedInt;
                break;
            default:
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh index type" << mesh.indexType();
                return {};
        }
    }

    /* 3.7.2.1 (Geometry § Meshes § Overview) says "Primitives specify one or
       more attributes"; we allow this in non-strict mode */
    if(!mesh.attributeCount()) {
        /* The count is specified only in the accessors, if we have none we
           can't preserve that information. */
        if(mesh.vertexCount()) {
            Error{} << "Trade::GltfSceneConverter::add(): attribute-less mesh with a non-zero vertex count is unrepresentable in glTF";
            return {};
        }

        if(configuration().value<bool>("strict")) {
            Error{} << "Trade::GltfSceneConverter::add(): attribute-less meshes are not valid glTF, set strict=false to allow them";
            return {};
        } else if(!(flags() & SceneConverterFlag::Quiet))
            Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing an attribute-less mesh";

    /* 3.7.2.1 (Geometry § Meshes § Overview) says "[count] MUST be non-zero";
       we allow this in non-strict mode. Attribute-less meshes in glTF
       implicitly have zero vertices, so don't warn twice in that case. */
    } else if(!mesh.vertexCount()) {
        if(configuration().value<bool>("strict")) {
            Error{} << "Trade::GltfSceneConverter::add(): meshes with zero vertices are not valid glTF, set strict=false to allow them";
            return {};
        } else if(!(flags() & SceneConverterFlag::Quiet))
            Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing a mesh with zero vertices";
    }

    /* Check and convert attributes. Generally, count of attributes in the
       output file doesn't need to match count of attributes in the input mesh
       (although that's the common case), so the array references original mesh
       attribute IDs. */
    struct GltfAttribute {
        Containers::String name;
        Containers::StringView accessorType;
        Int accessorComponentType;
        UnsignedInt originalId;
        UnsignedInt offset;
    };
    Containers::Array<GltfAttribute> gltfAttributes;
    arrayReserve(gltfAttributes, mesh.attributeCount());
    /* Separate counter for joint IDs and weights attribute because they can
       appear in any order. In the end they should end up being the same, as
       matching sizes are enforced by MeshData itself. */
    UnsignedInt jointIdsAttributeGroupCount = 0,
        weightsAttributeGroupCount = 0;
    for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i) {
        /** @todo option to skip unrepresentable attributes instead of failing
            the whole mesh */

        const MeshAttribute attributeName = mesh.attributeName(i);

        /* For backwards compatibility GltfImporter and AssimpImporter aliases
           the JointIds and Weights attributes to custom JOINTS and WEIGHTS.
           Skip them if they have the name defined and there's a corresponding
           builtin attribute. Not testing that they actually alias the builtin
           ones since it's non-trivial due to how they're converted to arrays,
           also not testing for expected type, this should be enough to cover
           the actual cases. */
        /** @todo remove once compatibilitySkinningAttributes is dropped in
            these plugins */
        {
            bool skip = false;
            for(const Containers::Pair<MeshAttribute, Containers::String>& j: _state->customMeshAttributes) {
                if(j.first() == attributeName &&
                   ((j.second() == "JOINTS"_s && mesh.hasAttribute(MeshAttribute::JointIds)) ||
                    (j.second() == "WEIGHTS"_s && mesh.hasAttribute(MeshAttribute::Weights))))
                {
                    skip = true;
                    break;
                }
            }
            if(skip)
                continue;
        }

        const VertexFormat format = mesh.attributeFormat(i);
        if(isVertexFormatImplementationSpecific(format)) {
            Error{} << "Trade::GltfSceneConverter::add(): implementation-specific vertex format" << Debug::hex << vertexFormatUnwrap(format) << "can't be exported";
            return {};
        }

        const UnsignedInt componentCount = vertexFormatComponentCount(format);

        /* Positions are always three-component, two-component positions would
           fail */
        Containers::String gltfAttributeName;
        if(attributeName == MeshAttribute::Position) {
            gltfAttributeName = Containers::String::nullTerminatedGlobalView("POSITION"_s);

            /* Half-float types and cross-byte-packed types not supported by
               glTF */
            if(format == VertexFormat::Vector3b ||
               format == VertexFormat::Vector3bNormalized ||
               format == VertexFormat::Vector3ub ||
               format == VertexFormat::Vector3ubNormalized ||
               format == VertexFormat::Vector3s ||
               format == VertexFormat::Vector3sNormalized ||
               format == VertexFormat::Vector3us ||
               format == VertexFormat::Vector3usNormalized) {
                _state->requiredExtensions |= GltfExtension::KhrMeshQuantization;
            } else if(format != VertexFormat::Vector3) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh position attribute format" << format;
                return {};
            }

        /* Normals are always three-component, Magnum doesn't have
           two-component normal packing at the moment */
        } else if(attributeName == MeshAttribute::Normal) {
            gltfAttributeName = Containers::String::nullTerminatedGlobalView("NORMAL"_s);

            /* Half-float types and cross-byte-packed types not supported by
               glTF */
            if(format == VertexFormat::Vector3bNormalized ||
               format == VertexFormat::Vector3sNormalized) {
                _state->requiredExtensions |= GltfExtension::KhrMeshQuantization;
            } else if(format != VertexFormat::Vector3) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh normal attribute format" << format;
                return {};
            }

        /* Tangents are always four-component. Because three-component
           tangents are also common, these will be exported as a custom
           attribute with a warning. */
        } else if(attributeName == MeshAttribute::Tangent && componentCount == 4) {
            gltfAttributeName = Containers::String::nullTerminatedGlobalView("TANGENT"_s);

            /* Half-float types and cross-byte-packed types not supported by
               glTF */
            if(format == VertexFormat::Vector4bNormalized ||
               format == VertexFormat::Vector4sNormalized) {
                _state->requiredExtensions |= GltfExtension::KhrMeshQuantization;
            } else if(format != VertexFormat::Vector4) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh tangent attribute format" << format;
                return {};
            }

        /* Texture coordinates are always two-component, Magnum doesn't have
           three-compoent / layered texture coordinates at the moment */
        } else if(attributeName == MeshAttribute::TextureCoordinates) {
            gltfAttributeName = Containers::String::nullTerminatedGlobalView("TEXCOORD"_s);

            /* Half-float types and cross-byte-packed types not supported by
               glTF */
            if(format == VertexFormat::Vector2b ||
               format == VertexFormat::Vector2bNormalized ||
               format == VertexFormat::Vector2ub ||
               format == VertexFormat::Vector2s ||
               format == VertexFormat::Vector2sNormalized ||
               format == VertexFormat::Vector2us) {
                /* Fail if we have non-flippable format and the Y-flip isn't
                   done in the material */
                if(!configuration().value<bool>("textureCoordinateYFlipInMaterial")) {
                    Error{} << "Trade::GltfSceneConverter::add(): non-normalized mesh texture coordinates can't be Y-flipped, enable textureCoordinateYFlipInMaterial for the whole file instead";
                    return {};
                }

                _state->requiredExtensions |= GltfExtension::KhrMeshQuantization;
            } else if(format != VertexFormat::Vector2 &&
                      format != VertexFormat::Vector2ubNormalized &&
                      format != VertexFormat::Vector2usNormalized) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh texture coordinate attribute format" << format;
                return {};
            }

        /* Colors are either three- or four-component */
        } else if(attributeName == MeshAttribute::Color) {
            gltfAttributeName = Containers::String::nullTerminatedGlobalView("COLOR"_s);

            /* Half-float types and cross-byte-packed types not supported by
               glTF */
            if(format != VertexFormat::Vector3 &&
               format != VertexFormat::Vector4 &&
               format != VertexFormat::Vector3ubNormalized &&
               format != VertexFormat::Vector4ubNormalized &&
               format != VertexFormat::Vector3usNormalized &&
               format != VertexFormat::Vector4usNormalized) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh color attribute format" << format;
                return {};
            }

        /* Joint IDs and weights are an array in Magnum, glTF however expects
           them to be four-component attributes, so their array size has to be
           a multiple of four. They're then split into multiple attributes. */
        } else if(attributeName == MeshAttribute::JointIds) {
            gltfAttributeName = Containers::String::nullTerminatedGlobalView("JOINTS"_s);

            /* All formats supported for JointIds by Magnum are also allowed
               here, so nothing to check. Just assert in case a new format
               would be added in the future */
            CORRADE_INTERNAL_ASSERT(
                format == VertexFormat::UnsignedByte ||
                format == VertexFormat::UnsignedShort ||
                /* Not allowed by the spec, but we allow it in non-strict mode
                   similarly as with ObjectId (checked below) */
                format == VertexFormat::UnsignedInt);

            const UnsignedInt arraySize = mesh.attributeArraySize(i);
            if(arraySize % 4 != 0) {
                Error{} << "Trade::GltfSceneConverter::add(): glTF only supports skin joint IDs with multiples of four elements, got" << arraySize;
                return {};
            }
        } else if(attributeName == MeshAttribute::Weights) {
            gltfAttributeName = Containers::String::nullTerminatedGlobalView("WEIGHTS"_s);

            /* Half-float types not supported by glTF */
            if(format != VertexFormat::Float &&
               format != VertexFormat::UnsignedByteNormalized &&
               format != VertexFormat::UnsignedShortNormalized) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh skin weights attribute format" << format;
                return {};
            }

            const UnsignedInt arraySize = mesh.attributeArraySize(i);
            if(arraySize % 4 != 0) {
                Error{} << "Trade::GltfSceneConverter::add(): glTF only supports skin weights with multiples of four elements, got" << arraySize;
                return {};
            }

        /* Otherwise it's a custom attribute where anything representable by
           glTF is allowed */
        } else {
            switch(attributeName) {
                /* All these are handled above already */
                /* LCOV_EXCL_START */
                case MeshAttribute::Position:
                case MeshAttribute::Normal:
                case MeshAttribute::TextureCoordinates:
                case MeshAttribute::Color:
                case MeshAttribute::JointIds:
                case MeshAttribute::Weights:
                    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
                /* LCOV_EXCL_STOP */

                case MeshAttribute::Tangent:
                    CORRADE_INTERNAL_ASSERT(componentCount == 3);
                    gltfAttributeName = Containers::String::nullTerminatedGlobalView("_TANGENT3"_s);
                    if(!(flags() & SceneConverterFlag::Quiet))
                        Warning{} << "Trade::GltfSceneConverter::add(): exporting three-component mesh tangents as a custom" << gltfAttributeName << "attribute";
                    break;

                case MeshAttribute::Bitangent:
                    gltfAttributeName = Containers::String::nullTerminatedGlobalView("_BITANGENT"_s);
                    if(!(flags() & SceneConverterFlag::Quiet))
                        Warning{} << "Trade::GltfSceneConverter::add(): exporting separate mesh bitangents as a custom" << gltfAttributeName << "attribute";
                    break;

                case MeshAttribute::ObjectId:
                    /* The returned view isn't global, but will stay in scope
                       until the configuration gets modified. Which won't
                       happen inside this function so we're fine. */
                    gltfAttributeName = Containers::String::nullTerminatedView(configuration().value<Containers::StringView>("objectIdAttribute"));
                    break;
            }

            /* For custom attributes pick an externally supplied name or
               generate one from the numeric value if not supplied */
            if(!gltfAttributeName) {
                CORRADE_INTERNAL_ASSERT(isMeshAttributeCustom(attributeName));
                for(const Containers::Pair<MeshAttribute, Containers::String>& j: _state->customMeshAttributes) {
                    if(j.first() == attributeName) {
                        /* Make a non-owning reference to avoid a copy */
                        gltfAttributeName = Containers::String::nullTerminatedView(j.second());
                        break;
                    }
                }
                if(!gltfAttributeName) {
                    gltfAttributeName = Utility::format("_{}", meshAttributeCustom(attributeName));
                    if(!(flags() & SceneConverterFlag::Quiet))
                        Warning{} << "Trade::GltfSceneConverter::add(): no name set for" << attributeName << Debug::nospace << ", exporting as" << gltfAttributeName;
                }
            }
        }

        /* Decide on accessor type. Joint IDs and weights are array attributes
           in Magnum so they're special-cased here. */
        Containers::StringView gltfAccessorType;
        if(attributeName == MeshAttribute::JointIds ||
           attributeName == MeshAttribute::Weights) {
            gltfAccessorType = "VEC4"_s;
        } else {
            const UnsignedInt vectorCount = vertexFormatVectorCount(format);
            if(vectorCount == 1) {
                if(componentCount == 1)
                    gltfAccessorType = "SCALAR"_s;
                else if(componentCount == 2)
                    gltfAccessorType = "VEC2"_s;
                else if(componentCount == 3)
                    gltfAccessorType = "VEC3"_s;
                else if(componentCount == 4)
                    gltfAccessorType = "VEC4"_s;
                else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            } else if(vectorCount == 2 && componentCount == 2) {
                gltfAccessorType = "MAT2"_s;
            } else if(vectorCount == 3 && componentCount == 3) {
                gltfAccessorType = "MAT3"_s;
            } else if(vectorCount == 4 && componentCount == 4) {
                gltfAccessorType = "MAT4"_s;
            } else {
                Error{} << "Trade::GltfSceneConverter::add(): unrepresentable mesh vertex format" << format;
                return {};
            }

            /* glTF requires matrices to be aligned to four bytes -- i.e.,
               using the Matrix2x2bNormalizedAligned,
               Matrix3x3bNormalizedAligned or Matrix3x3sNormalizedAligned
               formats instead of the formats missing the Aligned suffix.
               Fortunately we don't need to check each individually as we have
               a neat tool instead. */
            if(vectorCount != 1 && vertexFormatVectorStride(format) % 4 != 0) {
                Error{} << "Trade::GltfSceneConverter::add(): mesh matrix attributes are required to be four-byte-aligned but got" << format;
                return {};
            }
        }

        Int gltfAccessorComponentType;
        const VertexFormat componentFormat = vertexFormatComponentFormat(format);
        if(componentFormat == VertexFormat::Byte)
            gltfAccessorComponentType = Implementation::GltfTypeByte;
        else if(componentFormat == VertexFormat::UnsignedByte)
            gltfAccessorComponentType = Implementation::GltfTypeUnsignedByte;
        else if(componentFormat == VertexFormat::Short)
            gltfAccessorComponentType = Implementation::GltfTypeShort;
        else if(componentFormat == VertexFormat::UnsignedShort)
            gltfAccessorComponentType = Implementation::GltfTypeUnsignedShort;
        else if(componentFormat == VertexFormat::UnsignedInt) {
            /* UnsignedInt is supported only for indices, not attributes; we
               allow this in non-strict mode  */
            if(configuration().value<bool>("strict")) {
                Error{} << "Trade::GltfSceneConverter::add(): mesh attributes with" << format << "are not valid glTF, set strict=false to allow them";
                return {};
            } else if(!(flags() & SceneConverterFlag::Quiet))
                Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing a 32-bit integer attribute" << gltfAttributeName;

            gltfAccessorComponentType = Implementation::GltfTypeUnsignedInt;
        } else if(componentFormat == VertexFormat::Float)
            gltfAccessorComponentType = Implementation::GltfTypeFloat;
        else {
            Error{} << "Trade::GltfSceneConverter::add(): unrepresentable mesh vertex format" << format;
            return {};
        }

        /* Final checks on attribute weirdness */
        if(mesh.attributeStride(i) <= 0) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh attribute with stride" << mesh.attributeStride(i);
            return {};
        }

        /* Joint IDs and weights are array attributes, add one attribute for
           each group of four and number them based on the count of groups
           found so far. It's checked above that there's exactly a multiple of
           four in each attribute. */
        if(attributeName == MeshAttribute::JointIds ||
           attributeName == MeshAttribute::Weights) {
            UnsignedInt& attributeGroupCount =
                attributeName == MeshAttribute::JointIds ?
                    jointIdsAttributeGroupCount :
                    weightsAttributeGroupCount;
            const UnsignedInt formatSize = vertexFormatSize(format);
            for(UnsignedInt j = 0; j < mesh.attributeArraySize(i); j += 4) {
                arrayAppend(gltfAttributes, InPlaceInit,
                    Utility::format("{}_{}", gltfAttributeName, attributeGroupCount++),
                    gltfAccessorType,
                    gltfAccessorComponentType,
                    i,
                    j*formatSize);
            }

        /* Common handling for all other attributes */
        } else {
            /* If a builtin glTF numbered attribute, append an ID to the name.
               JOINTS and WEIGHTS were handled above already. */
            if(gltfAttributeName == "TEXCOORD"_s ||
               gltfAttributeName == "COLOR"_s)
            {
                gltfAttributeName = Utility::format("{}_{}", gltfAttributeName, mesh.attributeId(i));

            /* Otherwise, if it's a second or further duplicate attribute,
               underscore it if not already and append an ID as well -- e.g.
               second and third POSITION attribute becomes _POSITION_1 and
               _POSITION_2, secondary _OBJECT_ID becomes _OBJECT_ID_1 */
            } else if(const UnsignedInt attributeId = mesh.attributeId(i)) {
                gltfAttributeName = Utility::format(
                    gltfAttributeName.hasPrefix('_') ? "{}_{}" : "_{}_{}",
                    gltfAttributeName, attributeId);
            }

            if(mesh.attributeArraySize(i) != 0) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh attribute with array size" << mesh.attributeArraySize(i);
                return {};
            }

            arrayAppend(gltfAttributes, InPlaceInit,
                Utility::move(gltfAttributeName),
                gltfAccessorType,
                gltfAccessorComponentType,
                i, 0u);
        }
    }

    CORRADE_INTERNAL_ASSERT(jointIdsAttributeGroupCount == weightsAttributeGroupCount);

    /* Group attributes into (strided) buffer views in a way that satisfies
       glTF requirements, in particular vertex count times stride not leaking
       out of the buffer view. */

    /* Attributes sorted by their offset. Each item is (offset, original
       attribute ID). */
    Containers::ArrayView<Containers::Pair<std::size_t, std::size_t>> attributesSortedByOffset;
    /* Buffer views to create. Each is (minOffset, stride) with minOffset being
       the earliest offset of any attribute in that view. Conservatively
       allocating for a case where each attribute would be its own view, a
       prefix gets used. */
    Containers::ArrayView<Containers::Pair<std::size_t, std::size_t>> bufferViews;
    /* Buffer view indices assigned to particular attributes, pointing to
       `bufferViews` above. */
    Containers::ArrayView<UnsignedInt> bufferViewAssignments;
    Containers::ArrayTuple bufferViewAssignmentStorage{
        {NoInit, mesh.attributeCount(), attributesSortedByOffset},
        {NoInit, mesh.attributeCount(), bufferViews},
        {NoInit, mesh.attributeCount(), bufferViewAssignments},
    };

    /* Sort attributes by their offset to group them into (strided) buffer
       views.  */
    for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i)
        attributesSortedByOffset[i] = {mesh.attributeOffset(i), i};
    std::sort(attributesSortedByOffset.begin(), attributesSortedByOffset.end(), [](const Containers::Pair<std::size_t, std::size_t> a, const Containers::Pair<std::size_t, std::size_t> b) {
        return
            a.first() < b.first() ||
                (a.first() == b.first() && a.second() < b.second());
    });

    /* Make each buffer view encompass a contiguous range of attributes with
       the same stride as long as their relative offset together with type size
       fits into the stride. */
    std::size_t bufferViewOffset = 0;
    for(const Containers::Pair<std::size_t, std::size_t> offsetId: attributesSortedByOffset) {
        /* If we have no buffer views yet, attribute stride is different from
           the current buffer view stride, or attribute offset together with
           type size doesn't current buffer view stride anymore, add a new
           buffer view */
        if(bufferViewOffset == 0 ||
           /* Negative strides were disallowed above already so the cast is
              fine */
           std::size_t(mesh.attributeStride(offsetId.second())) != bufferViews[bufferViewOffset - 1].second() ||
            /* Implementation-specific vertex formats were disallowed above
               already so it's safe to call vertexFormatSize(). Make sure to
               include array size in the calculation for JointIds and
               Weights. */
           offsetId.first() + vertexFormatSize(mesh.attributeFormat(offsetId.second()))*(mesh.attributeArraySize(offsetId.second()) ? mesh.attributeArraySize(offsetId.second()) : 1) > bufferViews[bufferViewOffset - 1].first() + bufferViews[bufferViewOffset - 1].second())
        {
            bufferViews[bufferViewOffset++] = {
                offsetId.first(),
                std::size_t(mesh.attributeStride(offsetId.second()))
            };
        }

        /* Remember the buffer view assigned to this attribute */
        CORRADE_INTERNAL_ASSERT(bufferViewOffset > 0);
        bufferViewAssignments[offsetId.second()] = bufferViewOffset - 1;
    }

    /* Go over the buffer views (minOffset, maxOffset, stride) and check that
       they fit into the vertex data size. If not (for example if there is
       initial padding in an interleaved vertex), try to move their initial
       offset earlier. If that's still not enough (for example if there's both
       an initial and final padding, in which case MeshData doesn't require the
       final padding to be included in the data) pad the buffer. */
    const std::size_t vertexDataSize = mesh.vertexData().size();
    std::size_t vertexBufferPadding = 0;
    for(Containers::Pair<std::size_t, std::size_t>& bufferView: bufferViews.prefix(bufferViewOffset)) {
        /* If the view fits into the vertex buffer, nothing to adjust */
        const std::size_t bufferEnd = bufferView.first() + mesh.vertexCount()*bufferView.second();
        if(bufferEnd <= vertexDataSize)
            continue;

        /* Shift the buffer back, ideally to make `bufferEnd` align with
           `vertexDataSize`. Earlier it doesn't make sense because that could
           omit some data at the end. We can also only shift back so minOffset
           doesn't become negative. */
        const std::size_t shiftBack = Math::min(
            bufferEnd - vertexDataSize,
            bufferView.first());
        bufferView.first() -= shiftBack;

        /* If the shift wasn't enough, we had to pad at the end */
        if(bufferEnd - shiftBack > vertexDataSize) {
            vertexBufferPadding = bufferEnd - shiftBack - vertexDataSize;
            if((flags() & SceneConverterFlag::Verbose))
                Debug{} << "Trade::GltfSceneConverter::add(): vertex buffer was padded by" << vertexBufferPadding << "bytes to satisfy glTF buffer view requirements";
        }
    }

    /* At this point we're sure nothing will fail so we can start writing the
       JSON. Otherwise we'd end up with a partly-written JSON in case of an
       unsupported mesh, corruputing the output. */

    /* If we have an index buffer or at least one attribute and this is a first
       buffer view / accessor, open the array */
    if(mesh.isIndexed() || mesh.attributeCount()) {
        if(_state->gltfBufferViews.isEmpty())
            _state->gltfBufferViews.beginArray();
        if(_state->gltfAccessors.isEmpty())
            _state->gltfAccessors.beginArray();
    }

    CORRADE_INTERNAL_ASSERT(_state->meshes.size() == id);
    MeshProperties& meshProperties = arrayAppend(_state->meshes, InPlaceInit);
    {
        /* Index view and accessor if the mesh is indexed */
        if(mesh.isIndexed()) {
            /* § 3.6.2.4 requires that "the offset of an accessor [...] MUST be
               a multiple of the size of the accessor’s component type". The
               byteOffset could be something else for example if there's
               (unaligned) image data preceding it. */
            {
                const std::size_t indexTypeSize = meshIndexTypeSize(mesh.indexType());
                const std::size_t padding = indexTypeSize*((_state->buffer.size() + indexTypeSize - 1)/indexTypeSize) - _state->buffer.size();
                CORRADE_INTERNAL_ASSERT(padding <= 3);
                /** @todo any better API for this? Utility::fill()? this is
                    silly */
                for(char& i: arrayAppend(_state->buffer, NoInit, padding))
                    i = '\0';
            }

            /* Using indices() instead of indexData() to discard arbitrary
               padding before and after */
            /** @todo or put the whole thing there, consistently with
                vertexData()? */
            const Containers::ArrayView<char> indexData = arrayAppend(_state->buffer, mesh.indices().asContiguous());

            const std::size_t gltfBufferViewIndex = _state->gltfBufferViews.currentArraySize();
            const Containers::ScopeGuard gltfBufferView = _state->gltfBufferViews.beginObjectScope();
            _state->gltfBufferViews
                .writeKey("buffer"_s).write(0)
                /** @todo could be omitted if zero, is that useful for anything? */
                .writeKey("byteOffset"_s).write(indexData - _state->buffer)
                .writeKey("byteLength"_s).write(indexData.size())
                .writeKey("target"_s).write(Implementation::GltfTargetHintElementArray);
            if(configuration().value<bool>("accessorNames"))
                _state->gltfBufferViews.writeKey("name"_s).write(Utility::format(
                    name ? "mesh {0} ({1}) indices" : "mesh {0} indices",
                    id, name));

            const std::size_t gltfAccessorIndex = _state->gltfAccessors.currentArraySize();
            const Containers::ScopeGuard gltfAccessor = _state->gltfAccessors.beginObjectScope();
            _state->gltfAccessors
                .writeKey("bufferView"_s).write(gltfBufferViewIndex)
                /* bufferOffset is implicitly 0 */
                .writeKey("componentType"_s).write(gltfIndexType)
                .writeKey("count"_s).write(mesh.indexCount())
                .writeKey("type"_s).write("SCALAR"_s);
            if(configuration().value<bool>("accessorNames"))
                _state->gltfAccessors.writeKey("name"_s).write(Utility::format(
                    name ? "mesh {0} ({1}) indices" : "mesh {0} indices",
                    id, name));

            meshProperties.gltfIndices = gltfAccessorIndex;
        }

        /* § 3.6.2.4 requires that "For performance and compatibility reasons,
           [...] accessor.byteOffset and bufferView.byteStride MUST be
           multiples of 4". The byteOffset could be something else for example
           if there's (unaligned) image data preceding it, or an odd number of
           8- or 16-bit indices. Pad the buffer appropriately. */
        /** @todo enforce also 4-byte-aligned stride */
        {
            const std::size_t padding = 4*((_state->buffer.size() + 3)/4) - _state->buffer.size();
            CORRADE_INTERNAL_ASSERT(padding <= 3);
            /** @todo any better API for this? Utility::fill()? this is silly */
            for(char& i: arrayAppend(_state->buffer, NoInit, padding))
                i = '\0';
        }

        /* Vertex data, plus any padding after. The view needs to include also
           the padding so it can get sliced to strided views without asserts. */
        Containers::ArrayView<char> vertexData = arrayAppend(_state->buffer, NoInit, mesh.vertexData().size() + vertexBufferPadding);
        Utility::copy(mesh.vertexData(), vertexData.prefix(mesh.vertexData().size()));
        /** @todo any better API for this? Utility::fill()? this is silly */
        for(char& i: vertexData.exceptPrefix(mesh.vertexData().size()))
            i = '\0';

        /* Remember the base buffer view index to which `bufferViewAssignments`
           are relative to. If there are no buffer views, the buffer view
           array might not even be opened yet. There are also no attributes in
           that case, thus use a deliberately wrong value to catch accidental
           access. */
        const std::size_t gltfBaseBufferViewIndex = bufferViewOffset ?
            _state->gltfBufferViews.currentArraySize() : ~std::size_t{};

        /* Write buffer views (minOffset, maxOffset, stride) */
        for(const Containers::Pair<std::size_t, std::size_t> bufferView: bufferViews.prefix(bufferViewOffset)) {
            const Containers::ScopeGuard gltfBufferView = _state->gltfBufferViews.beginObjectScope();

            _state->gltfBufferViews
                .writeKey("buffer"_s).write(0)
                /* Byte offset could be omitted if zero but since that
                   happens only for the very first view in a buffer and we
                   have always at most one buffer, the minimal savings are
                   not worth the inconsistency */
                .writeKey("byteOffset"_s).write(vertexData - _state->buffer + bufferView.first())
                .writeKey("byteLength"_s).write(mesh.vertexCount()*bufferView.second())
                /* Byte stride could be omitted if there would be just one
                   tightly packed accessor (in which case it'd be implicitly
                   treated as tightly packed, same as in GL). Tracking count of
                   accessors assigned to each view and then also maintaining an
                   info about whether the single accessor is tightly-packed is
                   a lot of extra work and the gains from being able to
                   omit byteStride are dubious.

                   It could be somewhat doable by just tracking count of
                   strided accessors to each buffer view and omitting
                   byteStride if there's 0, but this would omit byteStride also
                   if there's multiple tightly-packed accessors (for example,
                   for an aliased attribute) and § 3.6.2.4 disallows that:
                   "When two or more vertex attribute accessors use the same
                   bufferView, its byteStride MUST be defined." */
                /** @todo if vertex count is zero, this value is higher than
                    byteLength, is that a problem? glTF explicitly disallows
                    byteLength == 0 so this is uncharted waters anyway :D */
                .writeKey("byteStride"_s).write(bufferView.second())
                .writeKey("target"_s).write(Implementation::GltfTargetHintArray);

            if(configuration().value<bool>("accessorNames"))
                _state->gltfBufferViews.writeKey("name"_s).write(Utility::format(
                    name ? "mesh {0} ({1}) vertices" : "mesh {0} vertices",
                    id, name));
        }

        /* Attribute views and accessors */
        for(const GltfAttribute& gltfAttribute: gltfAttributes) {
            const MeshAttribute attributeName = mesh.attributeName(gltfAttribute.originalId);
            const VertexFormat format = mesh.attributeFormat(gltfAttribute.originalId);

            /* Flip texture coordinates unless they're meant to be flipped in
               the material */
            if(attributeName == MeshAttribute::TextureCoordinates && !configuration().value<bool>("textureCoordinateYFlipInMaterial")) {
                CORRADE_INTERNAL_ASSERT(gltfAttribute.offset == 0);
                Containers::StridedArrayView1D<char> data{vertexData,
                    vertexData + mesh.attributeOffset(gltfAttribute.originalId),
                    mesh.vertexCount(), mesh.attributeStride(gltfAttribute.originalId)};
                if(format == VertexFormat::Vector2)
                    for(auto& c: Containers::arrayCast<Vector2>(data))
                        c.y() = 1.0f - c.y();
                else if(format == VertexFormat::Vector2ubNormalized)
                    for(auto& c: Containers::arrayCast<Vector2ub>(data))
                        c.y() = 255 - c.y();
                else if(format == VertexFormat::Vector2usNormalized)
                    for(auto& c: Containers::arrayCast<Vector2us>(data))
                        c.y() = 65535 - c.y();
                /* Other formats are not possible to flip, and thus have to be
                   flipped in the material instead. This was already checked
                   at the top, failing if textureCoordinateYFlipInMaterial
                   isn't set for those formats, so it should never get here. */
                else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            }

            const UnsignedInt gltfAccessorIndex = _state->gltfAccessors.currentArraySize();
            const Containers::ScopeGuard gltfAccessor = _state->gltfAccessors.beginObjectScope();
            _state->gltfAccessors
                .writeKey("bufferView"_s).write(gltfBaseBufferViewIndex + bufferViewAssignments[gltfAttribute.originalId]);
            /* Write byteOffset only if non-zero. Compared to byteStride in the
               buffer view above, this is easy to do, so why not. */
            if(const std::size_t gltfByteOffset = mesh.attributeOffset(gltfAttribute.originalId) + gltfAttribute.offset - bufferViews[bufferViewAssignments[gltfAttribute.originalId]].first())
                _state->gltfAccessors.writeKey("byteOffset"_s).write(gltfByteOffset);
            _state->gltfAccessors
                .writeKey("componentType"_s).write(gltfAttribute.accessorComponentType);
            if(isVertexFormatNormalized(format))
                _state->gltfAccessors.writeKey("normalized"_s).write(true);
            _state->gltfAccessors
                .writeKey("count"_s).write(mesh.vertexCount())
                .writeKey("type"_s).write(gltfAttribute.accessorType);

            /* Calculate and write min/max bounds that are required for
               POSITION accessors */
            if(attributeName == MeshAttribute::Position) {
                /* Cast to a float type for all cases, no precision loss should
                   happen with the packed types */
                Containers::Pair<Vector3, Vector3> minmax;
                if(format == VertexFormat::Vector3) {
                    minmax = Math::minmax(mesh.attribute<Vector3>(gltfAttribute.originalId));
                } else if(format == VertexFormat::Vector3b ||
                          format == VertexFormat::Vector3bNormalized) {
                    minmax = Containers::Pair<Vector3, Vector3>{Math::minmax(mesh.attribute<Vector3b>(gltfAttribute.originalId))};
                } else if(format == VertexFormat::Vector3ub ||
                          format == VertexFormat::Vector3ubNormalized) {
                    minmax = Containers::Pair<Vector3, Vector3>{Math::minmax(mesh.attribute<Vector3ub>(gltfAttribute.originalId))};
                } else if(format == VertexFormat::Vector3s ||
                          format == VertexFormat::Vector3sNormalized) {
                    minmax = Containers::Pair<Vector3, Vector3>{Math::minmax(mesh.attribute<Vector3s>(gltfAttribute.originalId))};
                } else if(format == VertexFormat::Vector3us ||
                          format == VertexFormat::Vector3usNormalized) {
                    minmax = Containers::Pair<Vector3, Vector3>{Math::minmax(mesh.attribute<Vector3us>(gltfAttribute.originalId))};
                } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

                /* A sane validator implementation would have some tolerance
                   built in, or at the very least allow the min/max bounds to
                   be slightly larger than the data, and then we could just
                   save some slightly padded floats. NOT THE CASE HERE THOUGH,
                   the official Khronos glTF validator basically demands that
                   all glTF generators use Grisu2 as their float-to-string
                   routines, in order to EXACTLY match the validator
                   expectations, and there's no intention to ever fix it:
                    https://github.com/KhronosGroup/glTF-Validator/issues/79
                    https://github.com/KhronosGroup/glTF-Validator/issues/173
                   Writing floats causes the validator to complain about each
                   and every min/max accessor, sometimes MULTIPLE TIMES because
                   the values can then be both different than its expectation
                   and the data outside of those bounds (no shit!!). Writing
                   doubles avoids most of these, yet there's still random
                   corner cases as described in the above issues. */
                _state->gltfAccessors
                    .writeKey("min"_s).writeArray(Vector3d{minmax.first()}.data())
                    .writeKey("max"_s).writeArray(Vector3d{minmax.second()}.data());
            }

            if(configuration().value<bool>("accessorNames"))
                _state->gltfAccessors.writeKey("name"_s).write(Utility::format(
                    name ? "mesh {0} ({1}) {2}" : "mesh {0} {2}",
                    id, name, gltfAttribute.name));

            arrayAppend(meshProperties.gltfAttributes, InPlaceInit, gltfAttribute.name, gltfAccessorIndex);
        }

        /* Triangles are a default */
        if(gltfMode != 4)
            meshProperties.gltfMode = gltfMode;
    }

    if(name)
        meshProperties.gltfName = name;

    return true;
}

namespace {

/* Remembers which attributes were accessed to subsequently handle ones that
   weren't */
struct MaskedMaterial {
    explicit MaskedMaterial(const MaterialData& material, Containers::MutableBitArrayView mask, UnsignedInt layer = 0): material(material), layer{layer}, mask{mask} {}

    Containers::Optional<UnsignedInt> findId(Containers::StringView name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(layer, name);
        if(!found)
            return {};

        mask.set(material.attributeDataOffset(layer) + *found);
        return found;
    }

    Containers::Optional<UnsignedInt> findId(MaterialAttribute name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(layer, name);
        if(!found)
            return {};

        mask.set(material.attributeDataOffset(layer) + *found);
        return found;
    }

    template<class T> Containers::Optional<T> find(Containers::StringView name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(layer, name);
        if(!found)
            return {};

        mask.set(material.attributeDataOffset(layer) + *found);
        return material.attribute<T>(layer, *found);
    }

    template<class T> Containers::Optional<T> find(MaterialAttribute name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(layer, name);
        if(!found)
            return {};

        mask.set(material.attributeDataOffset(layer) + *found);
        return material.attribute<T>(layer, *found);
    }

    template<class T> Containers::Optional<T> findBaseLayer(MaterialAttribute name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(0, name);
        if(!found)
            return {};

        mask.set(*found);
        return material.attribute<T>(0, *found);
    }

    const MaterialData& material;
    UnsignedInt layer;
    Containers::MutableBitArrayView mask;
};

}

bool GltfSceneConverter::doAdd(UnsignedInt, const MaterialData& material, const Containers::StringView name) {
    /* Check that all referenced textures are in bounds. Because enumerating
       all potentially used textures would be very prone to accidentally
       missing some, it goes through all attributes and matches ones that end
       with `Texture` and are builtin (i.e., start with an uppercase letter
       and are in a builtin layer). Custom attributes are not checked as they
       could use some totally different indexing scheme. */
    for(UnsignedInt layer = 0, layerMax = material.layerCount(); layer != layerMax; ++layer) {
        /* Custom and unnamed layers (except the base layer) are not checked
           either */
        const Containers::StringView layerName = material.layerName(layer);
        if(layer && (!layerName || !std::isupper(layerName.front())))
            continue;

        for(UnsignedInt i = 0, iMax = material.attributeCount(layer); i != iMax; ++i) {
            const Containers::StringView attributeName = material.attributeName(layer, i);
            if(!attributeName.hasSuffix("Texture"_s) ||
               !std::isupper(attributeName.front()) ||
               /* Ignore also Phong DiffuseTexture -- it isn't and never will
                  be exported on its own, it's only checked if is the same as
                  BaseColorTexture and if not it produces a warning */
               /** @todo remove this once GltfImporter no longer implements
                   phongMaterialFallback */
               attributeName == "DiffuseTexture"_s)
                continue;

            /* Assuming that builtin *Texture attributes have the right type.
               If you have a custom Texture attribute with an uppercase first
               letter, it's your fault. sorry. */
            CORRADE_INTERNAL_ASSERT(material.attributeType(layer, i) == MaterialAttributeType::UnsignedInt);
            const UnsignedInt index = material.attribute<UnsignedInt>(layer, i);
            if(index >= textureCount()) {
                Error e;
                e << "Trade::GltfSceneConverter::add(): material attribute" << attributeName;
                if(layer != 0) {
                    /* Nameless layers should have been skipped above */
                    CORRADE_INTERNAL_ASSERT(layerName);
                    e << "in layer" << layerName;
                }
                e << "references texture" << index << "but only" << textureCount() << "were added so far";
                return {};
            }

            /* If there's a layer, validate that it's in bounds as well. For 2D
               textures the layer count is implicitly 1, so the layer can only
               be 0. Again assuming the *TextureLayer attribute has the right
               type. If there's no layer attribute directly for the texture,
               check the material-layer-local and global fallbacks as well. */
            CORRADE_INTERNAL_ASSERT(textureCount() + 1 == _state->textureIdOffsets.size());
            Containers::String textureLayerAttributeName = attributeName + "Layer"_s;
            UnsignedInt materialLayerUsedForTextureLayer = layer;
            Containers::Optional<UnsignedInt> textureLayer = material.findAttribute<UnsignedInt>(layer, textureLayerAttributeName);
            if(!textureLayer) {
                textureLayerAttributeName = Containers::String::nullTerminatedGlobalView(materialAttributeName(MaterialAttribute::TextureLayer));
                textureLayer = material.findAttribute<UnsignedInt>(layer, MaterialAttribute::TextureLayer);
            }
            if(!textureLayer && layer) {
                materialLayerUsedForTextureLayer = 0;
                textureLayer = material.findAttribute<UnsignedInt>(0, MaterialAttribute::TextureLayer);
            }
            if(textureLayer) {
                const UnsignedInt textureLayerCount = _state->textureIdOffsets[index + 1] - _state->textureIdOffsets[index];
                if(*textureLayer >= textureLayerCount) {
                    Error e;
                    e << "Trade::GltfSceneConverter::add(): material attribute" << textureLayerAttributeName;
                    if(materialLayerUsedForTextureLayer != 0) {
                        /* Nameless layers should have been skipped above */
                        CORRADE_INTERNAL_ASSERT(layerName);
                        e << "in layer" << layerName;
                    }
                    e << "value" << *textureLayer << "out of range for" << textureLayerCount << "layers in texture" << index;
                    return {};
                }
            }
        }
    }

    /* Check that all textures are using a compatible packing */
    {
        const auto& pbrMetallicRoughnessMaterial = material.as<PbrMetallicRoughnessMaterialData>();
        if(pbrMetallicRoughnessMaterial.hasMetalnessTexture() != pbrMetallicRoughnessMaterial.hasRoughnessTexture()) {
            /** @todo turn this into a warning and ignore the lone texture in
                that case? */
            Error{} << "Trade::GltfSceneConverter::add(): can only represent a combined metallic/roughness texture or neither of them";
            return {};
        }
        if(pbrMetallicRoughnessMaterial.hasMetalnessTexture() && pbrMetallicRoughnessMaterial.hasRoughnessTexture() && !pbrMetallicRoughnessMaterial.hasNoneRoughnessMetallicTexture()) {
            /** @todo this message is confusing if swizzle is alright but e.g.
                Matrix or Coordinates are different */
            Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrMetallicRoughnessMaterial.metalnessTextureSwizzle() << Debug::nospace << "/" << Debug::nospace << Debug::packed << pbrMetallicRoughnessMaterial.roughnessTextureSwizzle() << "packing of a metallic/roughness texture";
            return {};
        }
        if(pbrMetallicRoughnessMaterial.hasAttribute(MaterialAttribute::NormalTexture) && pbrMetallicRoughnessMaterial.normalTextureSwizzle() != MaterialTextureSwizzle::RGB) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrMetallicRoughnessMaterial.normalTextureSwizzle() << "packing of a normal texture";
            return {};
        }
        if(pbrMetallicRoughnessMaterial.hasAttribute(MaterialAttribute::OcclusionTexture) && pbrMetallicRoughnessMaterial.occlusionTextureSwizzle() != MaterialTextureSwizzle::R) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrMetallicRoughnessMaterial.occlusionTextureSwizzle() << "packing of an occlusion texture";
            return {};
        }
    }
    if(material.hasLayer(MaterialLayer::ClearCoat)) {
        const auto& pbrClearCoatMaterial = material.as<PbrClearCoatMaterialData>();
        if(pbrClearCoatMaterial.hasAttribute(MaterialAttribute::LayerFactorTexture) &&
        pbrClearCoatMaterial.layerFactorTextureSwizzle() != MaterialTextureSwizzle::R) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrClearCoatMaterial.layerFactorTextureSwizzle() << "packing of a clear coat layer factor texture";
            return {};
        }
        if(pbrClearCoatMaterial.hasAttribute(MaterialAttribute::RoughnessTexture) &&
        pbrClearCoatMaterial.roughnessTextureSwizzle() != MaterialTextureSwizzle::G) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrClearCoatMaterial.roughnessTextureSwizzle() << "packing of a clear coat roughness texture";
            return {};
        }
        if(pbrClearCoatMaterial.hasAttribute(MaterialAttribute::NormalTexture) && pbrClearCoatMaterial.normalTextureSwizzle() != MaterialTextureSwizzle::RGB) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrClearCoatMaterial.normalTextureSwizzle() << "packing of a clear coat normal texture";
            return {};
        }
    }

    /* At this point we're sure nothing will fail so we can start writing the
       JSON. Otherwise we'd end up with a partly-written JSON in case of an
       unsupported mesh, corruputing the output. */

    /* If this is a first material, open the materials array */
    if(_state->gltfMaterials.isEmpty())
        _state->gltfMaterials.beginArray();

    const Containers::ScopeGuard gltfMaterial = _state->gltfMaterials.beginObjectScope();

    const bool keepDefaults = configuration().value<bool>("keepMaterialDefaults");

    /* A dedicated lambda as the matrix is also written for textures in custom
       layers */
    const auto writeTextureMatrix = [this, keepDefaults](const Containers::StringView textureMatrixAttribute, UnsignedInt layer, Containers::StringView layerName, const Matrix3& textureMatrix) {
        /* Arbitrary rotation not supported yet, as there's several equivalent
           decompositions for an arbitrary matrix and I'm too lazy to try to
           find the most minimal one each time. This way I can also get away
           with just reusing the diagonal signs for scaling. */
        const Matrix3 exceptRotation = Matrix3::translation(textureMatrix.translation())*Matrix3::scaling(textureMatrix.scaling()*Math::sign(textureMatrix.diagonal().xy()));
        if(!(flags() & SceneConverterFlag::Quiet) && exceptRotation != textureMatrix) {
            Warning w;
            w << "Trade::GltfSceneConverter::add(): material attribute" << textureMatrixAttribute;
            if(layer) {
                CORRADE_INTERNAL_ASSERT(layerName);
                w << "in layer" << layer << "(" << Debug::nospace << layerName << Debug::nospace << ")";
            }
            w << "rotation was not used";
        }

        /* Flip the matrix to have origin upper left */
        Matrix3 matrix =
            Matrix3::translation(Vector2::yAxis(1.0f))*
            Matrix3::scaling(Vector2::yScale(-1.0f))*
            exceptRotation;

        /* If material needs an Y-flip, the mesh doesn't have the texture
           coordinates flipped and thus we don't need to unflip them
           first */
        if(!configuration().value<bool>("textureCoordinateYFlipInMaterial"))
            matrix = matrix*
                Matrix3::translation(Vector2::yAxis(1.0f))*
                Matrix3::scaling(Vector2::yScale(-1.0f));

        if(keepDefaults || matrix != Matrix3{}) {
            _state->requiredExtensions |= GltfExtension::KhrTextureTransform;

            const Vector2 translation = matrix.translation();
            const Vector2 scaling = matrix.scaling()*Math::sign(matrix.diagonal().xy());

            _state->gltfMaterials.writeKey("extensions"_s)
                .beginObject()
                    .writeKey("KHR_texture_transform"_s).beginObject();

            if(keepDefaults || translation != Vector2{})
                _state->gltfMaterials
                    .writeKey("offset"_s).writeArray(translation.data());
            if(keepDefaults || scaling != Vector2{1.0f})
                _state->gltfMaterials
                    .writeKey("scale"_s).writeArray(scaling.data());

            _state->gltfMaterials
                    .endObject()
                .endObject();
        }
    };
    const auto writeTextureContents = [this, keepDefaults, &writeTextureMatrix](MaskedMaterial& maskedMaterial, const UnsignedInt layer, const UnsignedInt textureAttributeId, Containers::StringView prefix) {
        if(!prefix)
            prefix = maskedMaterial.material.attributeName(layer, textureAttributeId);

        /* Bounds of all textures should have been verified at the very top */
        const UnsignedInt texture = maskedMaterial.material.attribute<UnsignedInt>(layer, textureAttributeId);
        CORRADE_INTERNAL_ASSERT(texture < _state->textureIdOffsets.size());

        /* Texture layer. If there's no such attribute and there's neither a
           material-layer-local or a global fallback, it's implicitly 0. Layer
           index bounds (including fallback layer attributes) should have been
           verified at the very top as well. */
        Containers::Optional<UnsignedInt> textureLayer = maskedMaterial.find<UnsignedInt>(prefix + "Layer"_s);
        if(!textureLayer)
            textureLayer = maskedMaterial.find<UnsignedInt>(MaterialAttribute::TextureLayer);
        if(!textureLayer && layer)
            textureLayer = maskedMaterial.findBaseLayer<UnsignedInt>(MaterialAttribute::TextureLayer);
        if(!textureLayer)
            textureLayer = 0;
        CORRADE_INTERNAL_ASSERT(*textureLayer < _state->textureIdOffsets[texture + 1] - _state->textureIdOffsets[texture]);

        _state->gltfMaterials.writeKey("index"_s).write(_state->textureIdOffsets[texture] + *textureLayer);

        /* Texture coordinates. If there's no such attribute, check also a
           material-layer-local and global fallback. */
        Containers::Optional<UnsignedInt> textureCoordinates = maskedMaterial.find<UnsignedInt>(prefix + "Coordinates"_s);
        if(!textureCoordinates)
            textureCoordinates = maskedMaterial.find<UnsignedInt>(MaterialAttribute::TextureCoordinates);
        if(!textureCoordinates && layer)
            textureCoordinates = maskedMaterial.findBaseLayer<UnsignedInt>(MaterialAttribute::TextureCoordinates);
        if(textureCoordinates && (keepDefaults || *textureCoordinates != 0))
            _state->gltfMaterials.writeKey("texCoord"_s).write(*textureCoordinates);

        /* Texture matrix. If there's no such attribute, check also a
           material-layer-local and global fallback. */
        Containers::String textureMatrixAttribute = prefix + "Matrix"_s;
        Containers::Optional<Matrix3> textureMatrix = maskedMaterial.find<Matrix3>(textureMatrixAttribute);
        if(!textureMatrix) {
            textureMatrixAttribute =  Containers::String::nullTerminatedGlobalView(materialAttributeName(MaterialAttribute::TextureMatrix));
            textureMatrix = maskedMaterial.find<Matrix3>(MaterialAttribute::TextureMatrix);
        }
        if(!textureMatrix && layer) {
            textureMatrix = maskedMaterial.findBaseLayer<Matrix3>(MaterialAttribute::TextureMatrix);
        }

        /* If there's no matrix but we're told to Y-flip texture coordinates in
           the material, add an identity -- in writeTextureMatrix() it'll be
           converted to a Y-flipping one */
        if(!textureMatrix && configuration().value<bool>("textureCoordinateYFlipInMaterial"))
            textureMatrix.emplace();

        if(textureMatrix)
            writeTextureMatrix(textureMatrixAttribute, layer, maskedMaterial.material.layerName(layer), *textureMatrix);
    };
    const auto writeTexture = [this, &writeTextureContents](MaskedMaterial& maskedMaterial, const Containers::StringView name, const UnsignedInt layer, const UnsignedInt textureAttributeId, const Containers::StringView prefix) {
        _state->gltfMaterials.writeKey(name);
        const Containers::ScopeGuard gltfTexture = _state->gltfMaterials.beginObjectScope();

        writeTextureContents(maskedMaterial, layer, textureAttributeId, prefix);
    };

    /* Originally I wanted to go through all material attributes sequentially,
       looking for attributes in a sorted order similarly to how two sorted
       ranges get merged. Thus O(n), with unused attributes being collected
       during the sequential process. But since that process would write the
       output in a rather random way while the JSON writer is sequential, it
       would mean having one JsonWriter open per possible texture, per possible
       texture transform, etc., opening each object lazily, and then merging
       all the writers together again. Which is a lot potential for things to
       go wrong, and any advanced inter-attribute logic such as "don't write
       any texture if there is other parameters but no ID" would be extremely
       complicated given the attributes have to be accessed in a sorted order.

       So instead I go with a O(n log m) process and using a helper to mark
       accessed attributes in a bitfield. That's asymptotically slower, but has
       a much smaller constant overhead due to only needing a single
       JsonWriter, so probably still faster than the O(n) idea. */
    Containers::BitArray mask{ValueInit, material.attributeData().size()};
    MaskedMaterial maskedBaseMaterial{material, mask};

    /* A mask for layers, so empty layers are properly detected and reported as
       well. The first layer is accessed always, so mark it as such. */
    Containers::BitArray layerMask{ValueInit, material.layerCount()};
    layerMask.set(0);

    /* Metallic/roughness material properties. Write only if there's actually
       something; texture properties will get ignored if there's no texture. */
    {
        const auto baseColor = maskedBaseMaterial.find<Color4>(MaterialAttribute::BaseColor);
        const auto metalness = maskedBaseMaterial.find<Float>(MaterialAttribute::Metalness);
        const auto roughness = maskedBaseMaterial.find<Float>(MaterialAttribute::Roughness);
        const auto foundBaseColorTexture = maskedBaseMaterial.findId(MaterialAttribute::BaseColorTexture);
        /* It was checked above that the correct Metallic/Roughness packing
           is used, so we can check either just for the metalness texture or
           for the combined one -- the roughness texture attributes are then
           exactly the same */
        const auto foundMetalnessTexture = maskedBaseMaterial.findId(MaterialAttribute::MetalnessTexture);
        const auto foundNoneRoughnessMetallicTexture = maskedBaseMaterial.findId(MaterialAttribute::NoneRoughnessMetallicTexture);
        if((material.types() & MaterialType::PbrMetallicRoughness) ||
           (baseColor && (keepDefaults || *baseColor != 0xffffffff_rgbaf)) ||
           (metalness && (keepDefaults || Math::notEqual(*metalness, 1.0f))) ||
           (roughness && (keepDefaults || Math::notEqual(*roughness, 1.0f))) ||
           foundBaseColorTexture ||
           foundMetalnessTexture || foundNoneRoughnessMetallicTexture)
        {
            _state->gltfMaterials.writeKey("pbrMetallicRoughness"_s);
            const Containers::ScopeGuard gltfMaterialPbrMetallicRoughness = _state->gltfMaterials.beginObjectScope();

            if(baseColor && (keepDefaults || *baseColor != 0xffffffff_rgbaf))
                _state->gltfMaterials
                    .writeKey("baseColorFactor"_s).writeArray(baseColor->data());
            if(foundBaseColorTexture)
                writeTexture(maskedBaseMaterial, "baseColorTexture"_s, 0, *foundBaseColorTexture, {});

            if(metalness && (keepDefaults || Math::notEqual(*metalness, 1.0f)))
                _state->gltfMaterials
                    .writeKey("metallicFactor"_s).write(*metalness);
            if(roughness && (keepDefaults || Math::notEqual(*roughness, 1.0f)))
                _state->gltfMaterials
                    .writeKey("roughnessFactor"_s).write(*roughness);
            if(foundMetalnessTexture) {
                writeTexture(maskedBaseMaterial, "metallicRoughnessTexture"_s, 0, *foundMetalnessTexture, {});

                /* Mark the swizzles and roughness properties as used, if
                   present, by simply looking them up -- we checked they're
                   valid and consistent with metalness above */
                maskedBaseMaterial.findId(MaterialAttribute::MetalnessTextureSwizzle);
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTexture);
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTextureSwizzle);
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTextureMatrix);
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTextureCoordinates);
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTextureLayer);

            } else if(foundNoneRoughnessMetallicTexture) {
                writeTexture(maskedBaseMaterial, "metallicRoughnessTexture"_s, 0, *foundNoneRoughnessMetallicTexture, "MetalnessTexture"_s);

                /* Mark the roughness properties as used, if present, by simply
                   looking them up -- we checked they're consistent with
                   metalness above */
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTextureMatrix);
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTextureCoordinates);
                maskedBaseMaterial.findId(MaterialAttribute::RoughnessTextureLayer);
            }
        }
    }

    /* Normal texture properties; ignored if there's no texture. A lambda
       because the texture is used also in the ClearCoat layer. */
    const auto writeNormalTexture = [this, keepDefaults, &writeTextureContents](MaskedMaterial& maskedMaterial, const Containers::StringView name, const UnsignedInt layer, const UnsignedInt textureAttributeId) {
        _state->gltfMaterials.writeKey(name);
        const Containers::ScopeGuard gltfTexture = _state->gltfMaterials.beginObjectScope();

        writeTextureContents(maskedMaterial, layer, textureAttributeId, {});

        /* Mark the swizzle as used, if present, by simply looking it up -- we
           checked it's valid above */
        maskedMaterial.findId(MaterialAttribute::NormalTextureSwizzle);

        const auto normalTextureScale = maskedMaterial.find<Float>(MaterialAttribute::NormalTextureScale);
        if(normalTextureScale && (keepDefaults || Math::notEqual(*normalTextureScale, 1.0f)))
            _state->gltfMaterials
                .writeKey("scale"_s).write(*normalTextureScale);
    };
    if(const auto foundNormalTexture = maskedBaseMaterial.findId(MaterialAttribute::NormalTexture))
        writeNormalTexture(maskedBaseMaterial, "normalTexture"_s, 0, *foundNormalTexture);

    /* Occlusion texture properties; ignored if there's no texture */
    if(const auto foundOcclusionTexture = maskedBaseMaterial.findId(MaterialAttribute::OcclusionTexture)) {
        _state->gltfMaterials.writeKey("occlusionTexture"_s);
        const Containers::ScopeGuard gltfTexture = _state->gltfMaterials.beginObjectScope();

        writeTextureContents(maskedBaseMaterial, 0, *foundOcclusionTexture, {});

        /* Mark the swizzle as used, if present, by simply looking it up -- we
           checked it's valid above */
        maskedBaseMaterial.findId(MaterialAttribute::OcclusionTextureSwizzle);

        const auto occlusionTextureStrength = maskedBaseMaterial.find<Float>(MaterialAttribute::OcclusionTextureStrength);
        if(occlusionTextureStrength && (keepDefaults || Math::notEqual(*occlusionTextureStrength, 1.0f)))
            _state->gltfMaterials
                .writeKey("strength"_s).write(*occlusionTextureStrength);
    }

    /* Emissive factor */
    {
        const auto emissiveColor = maskedBaseMaterial.find<Color3>(MaterialAttribute::EmissiveColor);
        if(emissiveColor && (keepDefaults || *emissiveColor != 0x000000_rgbf))
            _state->gltfMaterials
                .writeKey("emissiveFactor"_s).writeArray(emissiveColor->data());
    }

    /* Emissive texture properties; ignored if there's no texture */
    if(const auto foundEmissiveTexture = maskedBaseMaterial.findId(MaterialAttribute::EmissiveTexture))
        writeTexture(maskedBaseMaterial, "emissiveTexture"_s, 0, *foundEmissiveTexture, {});

    /* Alpha mode and cutoff */
    {
        const auto alphaMask = maskedBaseMaterial.find<Float>(MaterialAttribute::AlphaMask);
        const auto alphaBlend = maskedBaseMaterial.find<bool>(MaterialAttribute::AlphaBlend);
        if(alphaBlend && *alphaBlend) {
            _state->gltfMaterials.writeKey("alphaMode"_s).write("BLEND"_s);
            /* Alpha mask ignored in this case */
        } else if(alphaMask) {
            _state->gltfMaterials.writeKey("alphaMode"_s).write("MASK"_s);
            if(keepDefaults || Math::notEqual(*alphaMask, 0.5f))
                _state->gltfMaterials.writeKey("alphaCutoff"_s).write(*alphaMask);
        } else if(alphaBlend && keepDefaults) {
            CORRADE_INTERNAL_ASSERT(!*alphaBlend);
            _state->gltfMaterials.writeKey("alphaMode"_s).write("OPAQUE"_s);
        }
    }

    /* Double sided */
    {
        const auto doubleSided = maskedBaseMaterial.find<bool>(MaterialAttribute::DoubleSided);
        if(doubleSided && (keepDefaults || *doubleSided))
            _state->gltfMaterials.writeKey("doubleSided"_s).write(*doubleSided);
    }

    /* Helper lambda to write material attributes in either unrecognized
       extensions or in extras */
    const auto writeMaterialAttribute = [this, &material](MaskedMaterial maskedMaterial, const Containers::StringView attributeName, const UnsignedInt layer, const UnsignedInt attribute) {
        const MaterialAttributeType attributeType = material.attributeType(layer, attribute);

        /* Ignore attributes of unrepresentable types. They'll be reported as
           unused at the end. */
        if(attributeType == MaterialAttributeType::Pointer ||
           attributeType == MaterialAttributeType::MutablePointer ||
           attributeType == MaterialAttributeType::Buffer ||
           attributeType == MaterialAttributeType::TextureSwizzle ||
           /* Matrices could be written as flat arrays but then it's impossible
              to infer the dimensions; and nested arrays are ugly and not used
              by any extension in practice */
           /** @todo revisit when extensions actually use them (introduce array
               attributes, for example, or point directly to the JSON) */
           attributeType == MaterialAttributeType::Matrix2x2 ||
           attributeType == MaterialAttributeType::Matrix2x3 ||
           attributeType == MaterialAttributeType::Matrix2x4 ||
           attributeType == MaterialAttributeType::Matrix3x2 ||
           attributeType == MaterialAttributeType::Matrix3x3 ||
           attributeType == MaterialAttributeType::Matrix3x4 ||
           attributeType == MaterialAttributeType::Matrix4x2 ||
           attributeType == MaterialAttributeType::Matrix4x3)
            return;

        /* Now it can't fail anymore, write the attribute */
        _state->gltfMaterials.writeKey(attributeName);
        switch(material.attributeType(layer, attribute)) {
            #define _ct(value, type)                            \
                case MaterialAttributeType::value:              \
                    _state->gltfMaterials.write(material.attribute<type>(layer, attribute)); \
                    break;
            #define _cc(type, cast)                            \
                case MaterialAttributeType::type:              \
                    _state->gltfMaterials.write(cast(material.attribute<type>(layer, attribute))); \
                    break;
            #define _c(type) _ct(type, type)
            #define _ca(type)                                   \
                case MaterialAttributeType::type:               \
                    _state->gltfMaterials.writeArray(material.attribute<type>(layer, attribute).data()); \
                    break;
            /* Wrap matrices with a line break after every column (they're
               column-major, so first column is first `Rows` items of the
               array) */
            #define _cm(type)                                   \
                case MaterialAttributeType::type:               \
                    _state->gltfMaterials.writeArray(material.attribute<type>(layer, attribute).data(), type::Rows); \
                    break;
            /* LCOV_EXCL_START */
            _ct(Bool, bool)
            _c(Float)
            _cc(Deg, Float)
            _cc(Rad, Float)
            _c(UnsignedInt)
            _c(Int)
            _c(UnsignedLong)
            _c(Long)
            _ca(Vector2)
            _ca(Vector2ui)
            _ca(Vector2i)
            _ca(Vector3)
            _ca(Vector3ui)
            _ca(Vector3i)
            _ca(Vector4)
            _ca(Vector4ui)
            _ca(Vector4i)
            _ct(String, Containers::StringView)
            /* LCOV_EXCL_STOP */
            #undef _c
            #undef _ct
            #undef _cc
            #undef _ca
            #undef _cm

            /* These were handled above already */
            /* LCOV_EXCL_START */
            case MaterialAttributeType::Pointer:
            case MaterialAttributeType::MutablePointer:
            case MaterialAttributeType::Buffer:
            case MaterialAttributeType::TextureSwizzle:
            case MaterialAttributeType::Matrix2x2:
            case MaterialAttributeType::Matrix2x3:
            case MaterialAttributeType::Matrix2x4:
            case MaterialAttributeType::Matrix3x2:
            case MaterialAttributeType::Matrix3x3:
            case MaterialAttributeType::Matrix3x4:
            case MaterialAttributeType::Matrix4x2:
            case MaterialAttributeType::Matrix4x3:
                CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            /* LCOV_EXCL_STOP */
        }

        /* If not skipped, mark the material as used */
        maskedMaterial.mask.set(material.attributeDataOffset(layer) + attribute);
    };

    /* Extensions -- write only if there's actually any */
    {
        const Containers::Optional<UnsignedInt> clearCoatLayerId = material.findLayerId(MaterialLayer::ClearCoat);

        /* Layers starting with a # are unrecognized extensions parsed by
           GltfImporter, we want to write those back */
        bool hasCustomExtensions = false;
        for(UnsignedInt i = 0; i != material.layerCount(); ++i) if(material.layerName(i).hasPrefix('#')) {
            hasCustomExtensions = true;
            break;
        }

        if((material.types() & MaterialType::Flat) ||
            clearCoatLayerId ||
            hasCustomExtensions
        ) {
            _state->gltfMaterials.writeKey("extensions"_s);
            const Containers::ScopeGuard gltfExtensions = _state->gltfMaterials.beginObjectScope();

            /* Flat material */
            if(material.types() & MaterialType::Flat) {
                _state->usedExtensions |= GltfExtension::KhrMaterialsUnlit;
                _state->gltfMaterials
                    .writeKey("KHR_materials_unlit"_s).beginObject().endObject();
            }

            /* Clear coat. Include if the layer is present, independently of
               being included in the MaterialTypes (which, except for Flat, is
               just a hint of what's in there) */
            if(clearCoatLayerId) {
                layerMask.set(*clearCoatLayerId);
                _state->usedExtensions |= GltfExtension::KhrMaterialsClearCoat;
                _state->gltfMaterials.writeKey("KHR_materials_clearcoat"_s);
                const Containers::ScopeGuard gltfClearCoat = _state->gltfMaterials.beginObjectScope();

                MaskedMaterial maskedClearCoatMaterial{material, mask, *clearCoatLayerId};

                /* Clear coat layer factor. Our default is 1.0 (so it doesn't
                   have to be always specified if a factor texture is present
                   as well) while glTF's is 0.0 so it has to be added even if
                   it's missing. */
                {
                    auto layerFactor = maskedClearCoatMaterial.find<Float>(MaterialAttribute::LayerFactor);
                    if(!layerFactor)
                        layerFactor = 1.0f;
                    if(keepDefaults || *layerFactor != 0.0f)
                        _state->gltfMaterials
                            .writeKey("clearcoatFactor"_s).write(*layerFactor);
                }

                /* Layer texture properties; ignored if there's no texture */
                if(const auto foundFactorTexture = maskedClearCoatMaterial.findId(MaterialAttribute::LayerFactorTexture)) {
                    writeTexture(maskedClearCoatMaterial, "clearcoatTexture"_s, *clearCoatLayerId, *foundFactorTexture, {});

                    /* Mark the swizzle as used, if present, by simply looking
                       it up -- we checked it's valid above */
                    maskedClearCoatMaterial.findId(MaterialAttribute::LayerFactorTextureSwizzle);
                }

                /* Clear coat roughness. Similarly to above, our default is 1.0
                   while glTF's is 0.0 so it has to be added even if it's
                   missing. */
                {
                    auto roughness = maskedClearCoatMaterial.find<Float>(MaterialAttribute::Roughness);
                    if(!roughness)
                        roughness = 1.0f;
                    if(keepDefaults || *roughness != 0.0f)
                        _state->gltfMaterials
                            .writeKey("clearcoatRoughnessFactor"_s).write(*roughness);
                }

                /* Roughness texture properties; ignored if there's no
                   texture */
                if(const auto foundRoughnessTexture = maskedClearCoatMaterial.findId(MaterialAttribute::RoughnessTexture)) {
                    writeTexture(maskedClearCoatMaterial, "clearcoatRoughnessTexture"_s, *clearCoatLayerId, *foundRoughnessTexture, {});

                    /* Mark the swizzle as used, if present, by simply looking
                       it up -- we checked it's valid above */
                    maskedClearCoatMaterial.findId(MaterialAttribute::RoughnessTextureSwizzle);
                }

                /* Clear coat normal texture */
                if(const auto foundNormalTexture = maskedClearCoatMaterial.findId(MaterialAttribute::NormalTexture))
                    writeNormalTexture(maskedClearCoatMaterial, "clearcoatNormalTexture"_s, *clearCoatLayerId, *foundNormalTexture);
            }

            /* Unrecognized extensions parsed by GltfImporter are starting with
               a #. We don't understand those either so no checking is
               performed, just write them in the same way as GltfImporter
               parsed them. */
            if(hasCustomExtensions) for(UnsignedInt layer = 0; layer != material.layerCount(); ++layer) {
                const Containers::StringView layerName = material.layerName(layer);
                if(!layerName.hasPrefix('#'))
                    continue;

                const Containers::StringView extensionName = layerName.exceptPrefix("#"_s);
                arrayAppend(_state->usedCustomExtensions, Containers::String::nullTerminatedGlobalView(extensionName));

                _state->gltfMaterials.writeKey(extensionName);
                const Containers::ScopeGuard gltfExtension = _state->gltfMaterials.beginObjectScope();

                layerMask.set(layer);
                MaskedMaterial maskedLayerMaterial{material, mask, layer};

                /* Go through all attributes in this layer and write them.
                   Since the layer is named, the first attribute should be the
                   layer name -- skip it. */
                CORRADE_INTERNAL_ASSERT(material.attributeCount(layer) > 0 && material.attributeName(layer, 0) == materialAttributeName(MaterialAttribute::LayerName));
                for(UnsignedInt attribute = 1; attribute != material.attributeCount(layer); ++attribute) {
                    /* GltfImporter writes attributes of custom extensions with
                       a lowercase name -- if it's uppercase it's fishy, better
                       just skip it */
                    const Containers::StringView attributeName = material.attributeName(layer, attribute);
                    if(std::isupper(attributeName.front()))
                        continue;

                    /* A texture, attempt to write as a texture object but
                       with graceful fallbacks if something doesn't match -- we
                       don't pretend to understand anything in custom
                       extensions */
                    if(attributeName.hasSuffix("Texture"_s)) {
                        /* Set the attribute as used. If we don't use it in the
                           end, we'll print a warning why, so another warning
                           about it not being used would be redundant noise. */
                        maskedLayerMaterial.mask.set(material.attributeDataOffset(layer) + attribute);

                        /* If the *Texture attribute isn't UnsignedInt, print a
                           warning and skip it altogether */
                        if(material.attributeType(layer, attribute) != MaterialAttributeType::UnsignedInt) {
                            if(!(flags() & SceneConverterFlag::Quiet))
                                Warning{} << "Trade::GltfSceneConverter::add(): custom material attribute" << attributeName << "in layer" << layer << "(" << Debug::nospace << layerName << Debug::nospace << ")" << "is" << material.attributeType(layer, attribute) << Debug::nospace << ", not writing a textureInfo object";
                            continue;
                        }

                        const UnsignedInt texture = material.attribute<UnsignedInt>(layer, attribute);

                        /* If the texture is not in bounds, print a warning and
                           skip it entirely -- it'd be skipped on import in
                           GltfImporter otherwise anyway */
                        if(texture >= textureCount()) {
                            if(!(flags() & SceneConverterFlag::Quiet))
                                Warning{} << "Trade::GltfSceneConverter::add(): custom material attribute" << attributeName << "in layer" << layer << "(" << Debug::nospace << layerName << Debug::nospace << ")" << "references texture" << texture << "but only" << textureCount() << "textures were added so far, skipping";
                            continue;
                        }

                        /* Look also for the layer attribute. If there's no
                           such attribute and there's neither a
                           material-layer-local or a global fallback, it's
                           implicitly 0. If it isn't UnsignedInt, print a
                           warning and use implicit 0 instead. The fallbacks
                           are builtin so they're always UnsignedInt. */
                        Containers::Optional<UnsignedInt> textureLayer;
                        UnsignedInt materialLayerUsedForTextureLayer = layer;
                        Containers::String layerAttributeName = attributeName + "Layer"_s;
                        if(const Containers::Optional<UnsignedInt> attributeId = maskedLayerMaterial.findId(layerAttributeName)) {
                            const MaterialAttributeType attributeType = material.attributeType(layer, *attributeId);
                            if(attributeType != MaterialAttributeType::UnsignedInt) {
                                if(!(flags() & SceneConverterFlag::Quiet))
                                    Warning{} << "Trade::GltfSceneConverter::add(): custom material attribute" << layerAttributeName << "in layer" << layer << "(" << Debug::nospace << layerName << Debug::nospace << ")" << "is" << attributeType << Debug::nospace << ", referencing layer 0 instead";
                                textureLayer = 0;
                            } else {
                                textureLayer = material.attribute<UnsignedInt>(layer, *attributeId);
                            }
                        }
                        if(!textureLayer) {
                            layerAttributeName = materialAttributeName(MaterialAttribute::TextureLayer);
                            textureLayer = maskedLayerMaterial.find<UnsignedInt>(MaterialAttribute::TextureLayer);
                        }
                        if(!textureLayer && layer) {
                            materialLayerUsedForTextureLayer = 0;
                            textureLayer = maskedLayerMaterial.findBaseLayer<UnsignedInt>(MaterialAttribute::TextureLayer);
                        }
                        if(!textureLayer)
                            textureLayer = 0;

                        /* If the layer is not in bounds, print a warning and
                           skip the texture entirely -- it'd be skipped on
                           import in GltfImporter otherwise anyway */
                        CORRADE_INTERNAL_ASSERT(textureCount() + 1 == _state->textureIdOffsets.size());
                        if(*textureLayer >= _state->textureIdOffsets[texture + 1] - _state->textureIdOffsets[texture]) {
                            if(!(flags() & SceneConverterFlag::Quiet)) {
                                Warning w;
                                w << "Trade::GltfSceneConverter::add(): material attribute" << layerAttributeName;
                                if(materialLayerUsedForTextureLayer)
                                    w << "in layer" << materialLayerUsedForTextureLayer << "(" << Debug::nospace << layerName << Debug::nospace << ")";
                                w << "value" << *textureLayer << "out of range for" << _state->textureIdOffsets[texture + 1] - _state->textureIdOffsets[texture] << "layers in texture" << texture << Debug::nospace << ", skipping";
                            }
                            continue;
                        }

                        /* Begin the texture object, write its index */
                        _state->gltfMaterials.writeKey(attributeName);
                        const Containers::ScopeGuard gltfTexture = _state->gltfMaterials.beginObjectScope();

                        _state->gltfMaterials.writeKey("index"_s).write(_state->textureIdOffsets[texture] + *textureLayer);

                        /* Texture coordinates. If there's no such attribute,
                           check also a material-layer-local and global
                           fallback. If it isn't UnsignedInt, print a warning
                           instead. The fallbacks are builtin so they're always
                           UnsignedInt. */
                        Containers::Optional<UnsignedInt> textureCoordinates;
                        {
                            const Containers::String coordinatesAttributeName = attributeName + "Coordinates"_s;
                            if(const Containers::Optional<UnsignedInt> attributeId = maskedLayerMaterial.findId(coordinatesAttributeName)) {
                                const MaterialAttributeType attributeType = material.attributeType(layer, *attributeId);
                                if(attributeType != MaterialAttributeType::UnsignedInt) {
                                    if(!(flags() & SceneConverterFlag::Quiet))
                                        Warning{} << "Trade::GltfSceneConverter::add(): custom material attribute" << coordinatesAttributeName << "in layer" << layer << "(" << Debug::nospace << layerName << Debug::nospace << ")" << "is" << attributeType << Debug::nospace << ", not exporting any texture coordinate set";
                                } else
                                    textureCoordinates = material.attribute<UnsignedInt>(layer, *attributeId);
                            }
                        }
                        if(!textureCoordinates)
                            textureCoordinates = maskedLayerMaterial.find<UnsignedInt>(MaterialAttribute::TextureCoordinates);
                        if(!textureCoordinates && layer)
                            textureCoordinates = maskedLayerMaterial.findBaseLayer<UnsignedInt>(MaterialAttribute::TextureCoordinates);
                        if(textureCoordinates && (keepDefaults || *textureCoordinates != 0))
                            _state->gltfMaterials.writeKey("texCoord"_s).write(*textureCoordinates);

                        /* Texture matrix. If there's no such attribute, check
                           also a material-layer-local and global fallback. If
                           they aren't UnsignedInt, print a
                           warning instead. The fallbacks are builtin so
                           they're always UnsignedInt. */
                        Containers::Optional<Matrix3> textureMatrix;
                        Containers::String textureMatrixAttributeName = attributeName + "Matrix"_s;
                        UnsignedInt materialLayerUsedForTextureMatrix = layer;
                        {
                            if(const Containers::Optional<UnsignedInt> attributeId = maskedLayerMaterial.findId(textureMatrixAttributeName)) {
                                const MaterialAttributeType attributeType = material.attributeType(layer, *attributeId);
                                if(attributeType != MaterialAttributeType::Matrix3x3) {
                                    if(!(flags() & SceneConverterFlag::Quiet))
                                        Warning{} << "Trade::GltfSceneConverter::add(): custom material attribute" << textureMatrixAttributeName << "in layer" << layer << "(" << Debug::nospace << layerName << Debug::nospace << ")" << "is" << attributeType << Debug::nospace << ", not exporting any texture transform";
                                } else
                                    textureMatrix = material.attribute<Matrix3>(layer, *attributeId);
                            }
                        }
                        if(!textureMatrix) {
                            textureMatrixAttributeName =  Containers::String::nullTerminatedGlobalView(materialAttributeName(MaterialAttribute::TextureMatrix));
                            textureMatrix = maskedLayerMaterial.find<Matrix3>(MaterialAttribute::TextureMatrix);
                        }
                        if(!textureMatrix && layer) {
                            materialLayerUsedForTextureMatrix = 0;
                            textureMatrix = maskedLayerMaterial.findBaseLayer<Matrix3>(MaterialAttribute::TextureMatrix);
                        }

                        /* If there's no matrix but we're told to Y-flip
                           texture coordinates in the material, add an identity
                           -- in writeTextureMatrix() it'll be converted to a
                           Y-flipping one */
                        if(!textureMatrix && configuration().value<bool>("textureCoordinateYFlipInMaterial"))
                            textureMatrix.emplace();

                        if(textureMatrix)
                            /* The layerName is used only if
                               materialLayerUsedForTextureMatrix is non-zero */
                            writeTextureMatrix(textureMatrixAttributeName, materialLayerUsedForTextureMatrix, layerName, *textureMatrix);

                        continue;
                    }

                    /* Ignore all other texture-related attributes. If they
                       were written by the above, that's good, if not, they'll
                       be reported as unused at the end. */
                    if(attributeName.hasSuffix("TextureCoordinates"_s) ||
                       attributeName.hasSuffix("TextureLayer"_s) ||
                       attributeName.hasSuffix("TextureMatrix"_s) ||
                       attributeName.hasSuffix("TextureSwizzle"_s))
                        continue;

                    writeMaterialAttribute(maskedLayerMaterial, attributeName, layer, attribute);
                }
            }
        }
    }

    /* Put all custom attributes from the base layer (if there are any) into
       extras. This is again consistent with what GltfImporter does. */
    {
        bool hasCustomAttributes = false;
        for(UnsignedInt attribute = 0; attribute != material.attributeCount(); ++attribute) if(!std::isupper(material.attributeName(attribute).front())) {
            hasCustomAttributes = true;
            break;
        }

        if(hasCustomAttributes) {
            _state->gltfMaterials.writeKey("extras"_s);
            const Containers::ScopeGuard gltfExtras = _state->gltfMaterials.beginObjectScope();

            for(UnsignedInt attribute = 0; attribute != material.attributeCount(); ++attribute) {
                const Containers::StringView attributeName = material.attributeName(attribute);
                if(std::isupper(attributeName.front()))
                    continue;

                writeMaterialAttribute(maskedBaseMaterial, attributeName, 0, attribute);
            }
        }
    }

    if(name)
        _state->gltfMaterials.writeKey("name"_s).write(name);

    /* For backwards compatibility GltfImporter copies BaseColor-related
       attributes to DiffuseColor etc. Mark them as used if they're the same
       so it doesn't warn about them being unused. If they're not the same, a
       warning should still be printed. */
    /** @todo remove once GltfImporter's phongMaterialFallback option is gone */
    {
        const auto baseColorId = material.findAttributeId(MaterialAttribute::BaseColor);
        const auto diffuseColorId = material.findAttributeId(MaterialAttribute::DiffuseColor);
        if(baseColorId && diffuseColorId && material.attribute<Color4>(*baseColorId) == material.attribute<Color4>(*diffuseColorId))
            maskedBaseMaterial.mask.set(*diffuseColorId);
    } {
        const auto baseColorTextureId = material.findAttributeId(MaterialAttribute::BaseColorTexture);
        const auto diffuseTextureId = material.findAttributeId(MaterialAttribute::DiffuseTexture);
        if(baseColorTextureId && diffuseTextureId && material.attribute<UnsignedInt>(*baseColorTextureId) == material.attribute<UnsignedInt>(*diffuseTextureId))
            maskedBaseMaterial.mask.set(*diffuseTextureId);
    }  {
        const auto baseColorTextureMatrixId = material.findAttributeId(MaterialAttribute::BaseColorTextureMatrix);
        const auto diffuseTextureMatrixId = material.findAttributeId(MaterialAttribute::DiffuseTextureMatrix);
        if(baseColorTextureMatrixId && diffuseTextureMatrixId && material.attribute<Matrix3>(*baseColorTextureMatrixId) == material.attribute<Matrix3>(*diffuseTextureMatrixId))
            maskedBaseMaterial.mask.set(*diffuseTextureMatrixId);
    } {
        const auto baseColorTextureCoordinatesId = material.findAttributeId(MaterialAttribute::BaseColorTextureCoordinates);
        const auto diffuseTextureCoordinatesId = material.findAttributeId(MaterialAttribute::DiffuseTextureCoordinates);
        if(baseColorTextureCoordinatesId && diffuseTextureCoordinatesId && material.attribute<UnsignedInt>(*baseColorTextureCoordinatesId) == material.attribute<UnsignedInt>(*diffuseTextureCoordinatesId))
            maskedBaseMaterial.mask.set(*diffuseTextureCoordinatesId);
    } {
        const auto baseColorTextureLayerId = material.findAttributeId(MaterialAttribute::BaseColorTextureLayer);
        const auto diffuseTextureLayerId = material.findAttributeId(MaterialAttribute::DiffuseTextureLayer);
        if(baseColorTextureLayerId && diffuseTextureLayerId && material.attribute<UnsignedInt>(*baseColorTextureLayerId) == material.attribute<UnsignedInt>(*diffuseTextureLayerId))
            maskedBaseMaterial.mask.set(*diffuseTextureLayerId);
    }

    /* Report unused attributes and layers */
    if(!(flags() & SceneConverterFlag::Quiet)) for(std::size_t layer = 0, layerMax = material.layerCount(); layer != layerMax; ++layer) {
        const Containers::StringView layerName = material.layerName(layer);

        /* If the whole layer is unused, print just a single warning instead of
           one warning for all attributes in the layer. This also causes
           warnings to be properly printed for empty layers. */
        if(!layerMask[layer]) {
            Warning w;
            w << "Trade::GltfSceneConverter::add(): material layer" << layer;
            if(layerName)
                w << "(" << Debug::nospace << layerName << Debug::nospace << ")";
            w << "was not used";
            continue;
        }

        /* If the layer was used, print warnings for just the attributes that
           weren't. Except for layer names, which are only used for lookup. */
        const UnsignedInt offset = material.attributeDataOffset(layer);
        for(std::size_t i = 0, iMax = material.attributeCount(layer); i != iMax; ++i) {
            /** @todo some "iterate unset bits" API for this? */
            if(!mask[offset + i]) {
                const Containers::StringView attributeName = material.attributeName(layer, i);
                if(attributeName == materialAttributeName(MaterialAttribute::LayerName))
                    continue;

                Warning w;
                w << "Trade::GltfSceneConverter::add(): material attribute" << attributeName;
                if(layer != 0) {
                    w << "in layer" << layer;
                    if(layerName)
                        w << "(" << Debug::nospace << layerName << Debug::nospace << ")";
                }

                w << "was not used";
            }
        }
    }

    return true;
}

bool GltfSceneConverter::doAdd(CORRADE_UNUSED const UnsignedInt id, const TextureData& texture, const Containers::StringView name) {
    GltfExtension textureExtension;
    UnsignedInt gltfImageId;
    if(texture.type() == TextureType::Texture2D) {
        CORRADE_INTERNAL_ASSERT(image2DCount() == _state->image2DIdsTextureExtensions.size());
        if(texture.image() >= _state->image2DIdsTextureExtensions.size()) {
            Error{} << "Trade::GltfSceneConverter::add(): texture references 2D image" << texture.image() << "but only" << _state->image2DIdsTextureExtensions.size() << "were added so far";
            return {};
        }

        gltfImageId = _state->image2DIdsTextureExtensions[texture.image()].first();
        textureExtension = _state->image2DIdsTextureExtensions[texture.image()].second();

    } else if(texture.type() == TextureType::Texture2DArray) {
        if(!configuration().value<bool>("experimentalKhrTextureKtx")) {
            Error{} << "Trade::GltfSceneConverter::add(): 2D array textures require experimentalKhrTextureKtx to be enabled";
            return {};
        }

        CORRADE_INTERNAL_ASSERT(image3DCount() == _state->image3DIdsTextureExtensionsLayerCount.size());
        if(texture.image() >= _state->image3DIdsTextureExtensionsLayerCount.size()) {
            Error{} << "Trade::GltfSceneConverter::add(): texture references 3D image" << texture.image() << "but only" << _state->image3DIdsTextureExtensionsLayerCount.size() << "were added so far";
            return {};
        }

        gltfImageId = _state->image3DIdsTextureExtensionsLayerCount[texture.image()].first();
        textureExtension = _state->image3DIdsTextureExtensionsLayerCount[texture.image()].second();

    } else {
        Error{} << "Trade::GltfSceneConverter::add(): expected a 2D or 2D array texture, got" << texture.type();
        return {};
    }

    /* Check if the wrapping mode is supported by glTF */
    for(const std::size_t i: {0, 1}) {
        if(texture.wrapping()[i] != SamplerWrapping::ClampToEdge &&
           texture.wrapping()[i] != SamplerWrapping::MirroredRepeat &&
           texture.wrapping()[i] != SamplerWrapping::Repeat) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported texture wrapping" << texture.wrapping()[i];
            return {};
        }
    }

    /* At this point we're sure nothing will fail so we can start writing the
       JSON. Otherwise we'd end up with a partly-written JSON in case of an
       unsupported mesh, corruputing the output. */

    /* Mark the extension as required. This is only done if an image actually
       gets referenced by a texture. */
    if(textureExtension != GltfExtension{}) {
        _state->requiredExtensions |= textureExtension;
        _state->usedExtensions &= ~textureExtension;
    }

    /* Calculate unique sampler identifier. If we already have it, reference
       its ID. Otherwise create a new one. */
    const UnsignedInt samplerIdentifier =
        UnsignedInt(texture.wrapping()[0]) << 16 |
        UnsignedInt(texture.wrapping()[1]) << 12 |
        UnsignedInt(texture.minificationFilter()) << 8 |
        UnsignedInt(texture.mipmapFilter()) << 4 |
        UnsignedInt(texture.magnificationFilter()) << 0;
    auto foundSampler = _state->uniqueSamplers.find(samplerIdentifier);
    if(foundSampler == _state->uniqueSamplers.end()) {
        /* If this is a first sampler, open the sampler array */
        if(_state->gltfSamplers.isEmpty())
            _state->gltfSamplers.beginArray();

        UnsignedInt gltfWrapping[2];
        for(const std::size_t i: {0, 1}) {
            switch(texture.wrapping()[i]) {
                case SamplerWrapping::ClampToEdge:
                    gltfWrapping[i] = Implementation::GltfWrappingClampToEdge;
                    break;
                case SamplerWrapping::MirroredRepeat:
                    gltfWrapping[i] = Implementation::GltfWrappingMirroredRepeat;
                    break;
                /* This is the default, so it could possibly be omitted.
                   However, because the filters don't have defaults defined (so
                   we're writing them always) and because we're deduplicating
                   the samplers in the file, omitting a single value doesn't
                   really make a difference in the resulting file size. */
                case SamplerWrapping::Repeat:
                    gltfWrapping[i] = Implementation::GltfWrappingRepeat;
                    break;
                /* Unsupported modes checked above already */
                default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            }
        }
        UnsignedInt gltfMinFilter;
        switch(texture.minificationFilter()) {
            case SamplerFilter::Nearest:
                gltfMinFilter = Implementation::GltfFilterNearest;
                break;
            case SamplerFilter::Linear:
                gltfMinFilter = Implementation::GltfFilterLinear;
                break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
        /* Using same enum decomposition trick as in Magnum/GL/Sampler.cpp */
        constexpr UnsignedInt GltfMipmapNearest = Implementation::GltfFilterNearestMipmapNearest & ~Implementation::GltfFilterNearest;
        constexpr UnsignedInt GltfMipmapLinear = Implementation::GltfFilterNearestMipmapLinear & ~Implementation::GltfFilterNearest;
        static_assert(
            (Implementation::GltfFilterLinear|GltfMipmapNearest) == Implementation::GltfFilterLinearMipmapNearest &&
            (Implementation::GltfFilterLinear|GltfMipmapLinear) == Implementation::GltfFilterLinearMipmapLinear,
            "unexpected glTF sampler filter constants");
        switch(texture.mipmapFilter()) {
            case SamplerMipmap::Base:
                /* Nothing */
                break;
            case SamplerMipmap::Nearest:
                gltfMinFilter |= GltfMipmapNearest;
                break;
            case SamplerMipmap::Linear:
                gltfMinFilter |= GltfMipmapLinear;
                break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
        UnsignedInt gltfMagFilter;
        switch(texture.magnificationFilter()) {
            case SamplerFilter::Nearest:
                gltfMagFilter = Implementation::GltfFilterNearest;
                break;
            case SamplerFilter::Linear:
                gltfMagFilter = Implementation::GltfFilterLinear;
                break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
        _state->gltfSamplers.beginObject()
            .writeKey("wrapS"_s).write(gltfWrapping[0])
            .writeKey("wrapT"_s).write(gltfWrapping[1])
            .writeKey("minFilter"_s).write(gltfMinFilter)
            .writeKey("magFilter"_s).write(gltfMagFilter)
        .endObject();

        foundSampler = _state->uniqueSamplers.emplace(samplerIdentifier, _state->uniqueSamplers.size()).first;
    }

    /* If this is a first texture, open the texture array */
    if(_state->gltfTextures.isEmpty())
        _state->gltfTextures.beginArray();

    CORRADE_INTERNAL_ASSERT(_state->textureIdOffsets.size() == id + 1);

    /* For 2D array textures there's one texture per layer */
    if(texture.type() == TextureType::Texture2DArray) {
        CORRADE_INTERNAL_ASSERT(textureExtension == GltfExtension::KhrTextureKtx);
        const Containers::StringView textureExtensionString = "KHR_texture_ktx"_s;

        const UnsignedInt layerCount = _state->image3DIdsTextureExtensionsLayerCount[texture.image()].third();
        for(UnsignedInt layer = 0; layer != layerCount; ++layer) {
            const Containers::ScopeGuard gltfTexture = _state->gltfTextures.beginObjectScope();

            _state->gltfTextures
                .writeKey("sampler"_s).write(foundSampler->second)
                .writeKey("extensions"_s).beginObject()
                    .writeKey(textureExtensionString).beginObject()
                        .writeKey("source"_s).write(gltfImageId)
                        .writeKey("layer"_s).write(layer)
                    .endObject()
                .endObject();

            if(name)
                _state->gltfTextures.writeKey("name"_s).write(name);
        }

    /* 2D texture is just one */
    } else if(texture.type() == TextureType::Texture2D) {
        const Containers::ScopeGuard gltfTexture = _state->gltfTextures.beginObjectScope();

        _state->gltfTextures.writeKey("sampler"_s).write(foundSampler->second);

        /* Image that doesn't need any extension (PNG or JPEG or whatever else
           with strict mode disabled), write directly */
        if(textureExtension == GltfExtension{}) {
            _state->gltfTextures
                .writeKey("source"_s).write(gltfImageId);

        /* Image with an extension, also mark given extension as required */
        } else {
            _state->requiredExtensions |= textureExtension;

            Containers::StringView textureExtensionString;
            switch(textureExtension) {
                case GltfExtension::ExtTextureWebP:
                    textureExtensionString = "EXT_texture_webp"_s;
                    break;
                case GltfExtension::KhrTextureBasisu:
                    textureExtensionString = "KHR_texture_basisu"_s;
                    break;
                /* Not checking for experimentalKhrTextureKtx here, this is only
                reachable if it was enabled when the image got added */
                case GltfExtension::KhrTextureKtx:
                    textureExtensionString = "KHR_texture_ktx"_s;
                    break;
                /* LCOV_EXCL_START */
                case GltfExtension::KhrMaterialsUnlit:
                case GltfExtension::KhrMaterialsClearCoat:
                case GltfExtension::KhrMeshQuantization:
                case GltfExtension::KhrTextureTransform:
                    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
                /* LCOV_EXCL_STOP */
            }
            CORRADE_INTERNAL_ASSERT(textureExtensionString);

            _state->gltfTextures
                .writeKey("extensions"_s).beginObject()
                    .writeKey(textureExtensionString).beginObject()
                        .writeKey("source"_s).write(gltfImageId)
                    .endObject()
                .endObject();
        }

        if(name)
            _state->gltfTextures.writeKey("name"_s).write(name);

    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    arrayAppend(_state->textureIdOffsets, _state->gltfTextures.currentArraySize());

    return true;
}

namespace {

Containers::Pointer<AbstractImageConverter> loadAndInstantiateImageConverter(PluginManager::Manager<AbstractSceneConverter>* manager, const Containers::StringView plugin, const SceneConverterFlags flags, Utility::ConfigurationGroup& configuration, const ImageConverterFeatures expectedFeatures) {
    /* Get the image converter plugin through an external image converter
       manager */
    PluginManager::Manager<AbstractImageConverter>* imageConverterManager;
    if(!manager || !(imageConverterManager = manager->externalManager<AbstractImageConverter>())) {
        Error{} << "Trade::GltfSceneConverter::add(): the plugin must be instantiated with access to plugin manager that has a registered image converter manager in order to convert images";
        return {};
    }
    Containers::Pointer<AbstractImageConverter> imageConverter = imageConverterManager->loadAndInstantiate(plugin);
    if(!imageConverter) {
        Error{} << "Trade::GltfSceneConverter::add(): can't load" << plugin << "for image conversion";
        return {};
    }

    /** @todo imageConverterFallback option[s] to save multiple image formats;
        bundleImageFallbacks to have them externally (yay!) */

    /* Propagate flags that are common between scene and image converters */
    if(flags & SceneConverterFlag::Verbose)
        imageConverter->addFlags(ImageConverterFlag::Verbose);
    if(flags & SceneConverterFlag::Quiet)
        imageConverter->addFlags(ImageConverterFlag::Quiet);

    /* Propagate configuration values */
    Utility::ConfigurationGroup& imageConverterConfiguration = imageConverter->configuration();
    for(const Containers::Pair<Containers::StringView, Containers::StringView> value: configuration.group("imageConverter")->values()) {
        if(!(flags & SceneConverterFlag::Quiet) && !imageConverterConfiguration.hasValue(value.first()))
            Warning{} << "Trade::GltfSceneConverter::add(): option" << value.first() << "not recognized by" << plugin;

        imageConverterConfiguration.setValue(value.first(), value.second());
    }
    /** @todo made obsolete by magnum-sceneconverter's --set option in May
        2023, wait ~6 months and include this only in deprecated builds */
    if(configuration.group("imageConverter")->hasGroups()) {
        if(!(flags & SceneConverterFlag::Quiet))
            Warning{} << "Trade::GltfSceneConverter::add(): image converter configuration group propagation not implemented yet, ignoring";
    }

    if(!(imageConverter->features() >= expectedFeatures)) {
        Error{} << "Trade::GltfSceneConverter::add():" << plugin << "doesn't support" << expectedFeatures;
        return {};
    }

    return imageConverter;
}

}

template<UnsignedInt dimensions> bool GltfSceneConverter::convertAndWriteImage(const UnsignedInt id, const Containers::StringView name, AbstractImageConverter& imageConverter, const ImageData<dimensions>& image, bool bundleImages) {
    /* Only one of these two is filled */
    Containers::ArrayView<char> imageData;
    Containers::String imageFilename;
    if(bundleImages) {
        const Containers::Optional<Containers::Array<char>> out = imageConverter.convertToData(image);
        if(!out) {
            Error{} << "Trade::GltfSceneConverter::add(): can't convert an image";
            return {};
        }

        imageData = arrayAppend(_state->buffer, *out);
    } else {
        /* All existing image converters that return a MIME type return an
           extension as well, so we can (currently) get away with an assert.
           Might need to be revisited eventually. */
        const Containers::String extension = imageConverter.extension();
        CORRADE_INTERNAL_ASSERT(extension);

        if(!_state->filename) {
            Error{} << "Trade::GltfSceneConverter::add(): can only write a glTF with external images if converting to a file";
            return {};
        }

        imageFilename = Utility::format("{}.{}.{}",
            Utility::Path::splitExtension(*_state->filename).first(),
            id,
            extension);

        if(!imageConverter.convertToFile(image, imageFilename)) {
            Error{} << "Trade::GltfSceneConverter::add(): can't convert an image file";
            return {};
        }
    }

    /* At this point we're sure nothing will fail so we can start writing the
       JSON. Otherwise we'd end up with a partly-written JSON in case of an
       unsupported mesh, corruputing the output. */

    /* If this is a first image, open the images array */
    if(_state->gltfImages.isEmpty())
        _state->gltfImages.beginArray();

    const Containers::ScopeGuard gltfImage = _state->gltfImages.beginObjectScope();

    /* Bundled image, needs a buffer view and a MIME type */
    if(bundleImages) {
        /* The caller should have already checked the MIME type is not empty */
        const Containers::String mimeType = imageConverter.mimeType();
        CORRADE_INTERNAL_ASSERT(mimeType);

        /* If this is a first buffer view, open the buffer view array */
        if(_state->gltfBufferViews.isEmpty())
            _state->gltfBufferViews.beginArray();

        /* Reference the image data from a buffer view */
        const std::size_t gltfBufferViewIndex = _state->gltfBufferViews.currentArraySize();
        const Containers::ScopeGuard gltfBufferView = _state->gltfBufferViews.beginObjectScope();
        _state->gltfBufferViews
            .writeKey("buffer"_s).write(0)
            /** @todo could be omitted if zero, is that useful for anything? */
            .writeKey("byteOffset"_s).write(imageData - _state->buffer)
            .writeKey("byteLength"_s).write(imageData.size());
        if(configuration().value<bool>("accessorNames"))
            _state->gltfBufferViews.writeKey("name"_s).write(Utility::format(
                name ? "image {0} ({1})" : "image {0}", id, name));

        /* Reference the buffer view from the image */
        _state->gltfImages
            .writeKey("mimeType"_s).write(mimeType)
            .writeKey("bufferView"_s).write(gltfBufferViewIndex);

    /* External image, needs a URI and a file extension */
    } else {
        /* Reference the file from the image. Writing just the filename as the
           two files are expected to be next to each other. */
        _state->gltfImages
            .writeKey("uri"_s).write(Utility::Path::filename(imageFilename));
    }

    if(name)
        _state->gltfImages.writeKey("name"_s).write(name);

    return true;
}

bool GltfSceneConverter::doAdd(const UnsignedInt id, const ImageData2D& image, const Containers::StringView name) {
    /** @todo does it make sense to check for ImageFlag2D::Array here? glTF
        doesn't really care I think, and the image converters will warn on
        their own if that metadata is about to get lost */

    /* Decide whether to bundle images or save them externally. If not
       explicitly specified, bundle them for binary files and save externally
       for *.gltf. */
    const bool bundleImages =
        configuration().value<Containers::StringView>("bundleImages") ?
        configuration().value<bool>("bundleImages") : _state->binary;

    /* Decide on features we need */
    ImageConverterFeatures expectedFeatures;
    if(image.isCompressed())
        expectedFeatures |= bundleImages ?
            ImageConverterFeature::ConvertCompressed2DToData :
            ImageConverterFeature::ConvertCompressed2DToFile;
    else
        expectedFeatures |= bundleImages ?
            ImageConverterFeature::Convert2DToData :
            ImageConverterFeature::Convert2DToFile;

    /* Load the plugin, propagate flags & configuration. If it fails, it
       printed a message already, so just return. */
    const Containers::StringView imageConverterPluginName = configuration().value<Containers::StringView>("imageConverter");
    Containers::Pointer<AbstractImageConverter> imageConverter = loadAndInstantiateImageConverter(manager(), imageConverterPluginName, flags(), configuration(), expectedFeatures);
    if(!imageConverter)
        return {};

    /* Use a MIME type to decide what glTF extension (if any) to use to
       reference the image from a texture. Could also use the file extension,
       but a MIME type is more robust and all image converter plugins except
       Basis Universal have it. */
    const Containers::String mimeType = imageConverter->mimeType();
    GltfExtension extension;
    if(mimeType == "image/jpeg"_s ||
       mimeType == "image/png"_s) {
        extension = GltfExtension{};
    } else if(mimeType == "image/webp"_s) {
        extension = GltfExtension::ExtTextureWebP;
    /** @todo some more robust way to detect if Basis-encoded KTX image is
        produced? waiting until the image is produced and then parsing the
        header is insanely complicated :( */
    } else if(mimeType == "image/ktx2"_s && imageConverterPluginName == "BasisKtxImageConverter"_s) {
        extension = GltfExtension::KhrTextureBasisu;
    } else if(mimeType == "image/ktx2"_s && configuration().value<bool>("experimentalKhrTextureKtx")) {
        extension = GltfExtension::KhrTextureKtx;
    /** @todo MSFT_texture_dds, once we have converters */
    } else {
        if(!mimeType) {
            Error{} << "Trade::GltfSceneConverter::add():" << imageConverterPluginName << "doesn't specify any MIME type, can't save an image";
            return {};
        }

        if(!(flags() & SceneConverterFlag::Quiet) && mimeType == "image/ktx2"_s && !configuration().value<bool>("experimentalKhrTextureKtx"))
            Warning{} << "Trade::GltfSceneConverter::add(): KTX2 images can be saved using the KHR_texture_ktx extension, enable experimentalKhrTextureKtx to use it";

        if(configuration().value<bool>("strict")) {
            Error{} << "Trade::GltfSceneConverter::add():" << mimeType << "is not a valid MIME type for a glTF image, set strict=false to allow it";
            return {};
        } else if(!(flags() & SceneConverterFlag::Quiet))
            Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing" << mimeType << "MIME type for an image";

        extension = GltfExtension{};
    }

    const UnsignedInt gltfImageId = image2DCount() + image3DCount();
    CORRADE_INTERNAL_ASSERT(gltfImageId == (_state->gltfImages.isEmpty() ? 0 : _state->gltfImages.currentArraySize()));

    /* If the image writing fails due to an error, don't add any extensions
       -- otherwise we'd blow up on the asserts below when adding the next
       image */
    if(!convertAndWriteImage(id, name, *imageConverter, image, bundleImages))
        return false;

    CORRADE_INTERNAL_ASSERT(_state->image2DIdsTextureExtensions.size() == id);
    arrayAppend(_state->image2DIdsTextureExtensions, InPlaceInit, gltfImageId, extension);

    /* Mark the extension as used. As required will be marked only if
       referenced by a texture. */
    if(extension != GltfExtension{})
        _state->usedExtensions |= extension;

    return true;
}

bool GltfSceneConverter::doAdd(const UnsignedInt id, const ImageData3D& image, const Containers::StringView name) {
    /* If not set, 3D image conversion isn't even advertised */
    CORRADE_INTERNAL_ASSERT(configuration().value<bool>("experimentalKhrTextureKtx"));

    if((image.flags() & (ImageFlag3D::Array|ImageFlag3D::CubeMap)) != ImageFlag3D::Array) {
        Error{} << "Trade::GltfSceneConverter::add(): expected a 2D array image but got" << (image.flags() & (ImageFlag3D::Array|ImageFlag3D::CubeMap));
        return {};
    }

    /* Decide whether to bundle images or save them externally. If not
       explicitly specified, bundle them for binary files and save externally
       for *.gltf. */
    const bool bundleImages =
        configuration().value<Containers::StringView>("bundleImages") ?
        configuration().value<bool>("bundleImages") : _state->binary;

    /* Decide on features we need */
    ImageConverterFeatures expectedFeatures;
    if(image.isCompressed())
        expectedFeatures |= bundleImages ?
            ImageConverterFeature::ConvertCompressed3DToData :
            ImageConverterFeature::ConvertCompressed3DToFile;
    else
        expectedFeatures |= bundleImages ?
            ImageConverterFeature::Convert3DToData :
            ImageConverterFeature::Convert3DToFile;

    /* Load the plugin, propagate flags & configuration. If it fails, it
       printed a message already, so just return. */
    const Containers::StringView imageConverterPluginName = configuration().value<Containers::StringView>("imageConverter");
    Containers::Pointer<AbstractImageConverter> imageConverter = loadAndInstantiateImageConverter(manager(), imageConverterPluginName, flags(), configuration(), expectedFeatures);
    if(!imageConverter)
        return {};

    /* Use a MIME type to decide what glTF extension (if any) to use to
       reference the image from a texture. Could also use the file extension,
       but a MIME type is more robust and all image converter plugins except
       Basis Universal have it. */
    const Containers::String mimeType = imageConverter->mimeType();
    GltfExtension extension;
    if(mimeType == "image/ktx2"_s) {
        extension = GltfExtension::KhrTextureKtx;
    } else {
        if(!mimeType) {
            Error{} << "Trade::GltfSceneConverter::add():" << imageConverterPluginName << "doesn't specify any MIME type, can't save an image";
            return {};
        }

        Error{} << "Trade::GltfSceneConverter::add():" << mimeType << "is not a valid MIME type for a 3D glTF image";
        return {};
    }

    const UnsignedInt gltfImageId = image2DCount() + image3DCount();
    CORRADE_INTERNAL_ASSERT(gltfImageId == (_state->gltfImages.isEmpty() ? 0 : _state->gltfImages.currentArraySize()));

    /* If the image writing fails due to an error, don't add any extensions
       -- otherwise we'd blow up on the asserts below when adding the next
       image */
    if(!convertAndWriteImage(id, name, *imageConverter, image, bundleImages))
        return false;

    CORRADE_INTERNAL_ASSERT(_state->image3DIdsTextureExtensionsLayerCount.size() == id);
    arrayAppend(_state->image3DIdsTextureExtensionsLayerCount, InPlaceInit, gltfImageId, extension, UnsignedInt(image.size().z()));

    /* Mark the extension as used. As required will be marked only if
       referenced by a texture. */
    if(extension != GltfExtension{})
        _state->usedExtensions |= extension;

    return true;
}

}}

CORRADE_PLUGIN_REGISTER(GltfSceneConverter, Magnum::Trade::GltfSceneConverter,
    MAGNUM_TRADE_ABSTRACTSCENECONVERTER_PLUGIN_INTERFACE)
