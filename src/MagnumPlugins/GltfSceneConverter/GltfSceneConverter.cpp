/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/JsonWriter.h>
#include <Corrade/Utility/Macros.h> /* CORRADE_UNUSED */
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/ArrayAllocator.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/SceneData.h>

#include "MagnumPlugins/GltfImporter/Gltf.h"

/* We'd have to endian-flip everything that goes into buffers, plus the binary
   glTF headers, etc. Too much work, hard to automatically test because the
   HW is hard to get. */
#ifdef CORRADE_TARGET_BIG_ENDIAN
#error this code will not work on Big Endian, sorry
#endif

namespace Magnum { namespace Trade {

namespace {

enum class GltfExtension {
    KhrMaterialsUnlit = 1 << 0,
    KhrMeshQuantization = 1 << 1,
    KhrTextureBasisu = 1 << 2,
    KhrTextureKtx = 1 << 3,
    KhrTextureTransform = 1 << 4
};
typedef Containers::EnumSet<GltfExtension> GltfExtensions;
CORRADE_ENUMSET_OPERATORS(GltfExtensions)

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
    Containers::Array<Containers::Pair<UnsignedShort, Containers::String>> customMeshAttributes;
    /* Object names */
    Containers::Array<Containers::String> objectNames;
    /* Scene field names */
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

    /* Because in glTF a material is tightly coupled with a mesh instead of
       being only assigned from a scene node, all meshes go to this array first
       and are written to the file together with a material assignment at the
       end.

       If a mesh is referenced from a scene, it goes into
       meshMaterialAssignments, where the first is index into the meshes array
       and second is the material (or -1 if no material). The item index is
       glTF mesh ID, which is referenced by the scene. Meshes not referenced
       in the scene are not referenced from meshMaterialAssignments and are
       written at the very end. */
    Containers::Array<MeshProperties> meshes;
    Containers::Array<Containers::Pair<UnsignedInt, Int>> meshMaterialAssignments;

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
        if(const Containers::StringView generator = configuration().value<Containers::StringView>("generator"_s))
            json.writeKey("generator"_s).write(generator);
    }

    /* Used and required extensions */
    {
        /** @todo FFS what the stone age types here */
        std::vector<Containers::StringView> extensionsUsed = configuration().values<Containers::StringView>("extensionUsed");
        std::vector<Containers::StringView> extensionsRequired = configuration().values<Containers::StringView>("extensionRequired");

        const auto contains = [](Containers::ArrayView<const Containers::StringView> extensions, Containers::StringView extension) {
            for(const Containers::StringView i: extensions)
                if(i == extension) return true;
            return false;
        };
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
            json.writeKey("uri"_s).write(Utility::Path::split(bufferFilename).second());
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

        const auto writeMesh = [](Utility::JsonWriter& json, const MeshProperties& mesh, Int material) {
            const Containers::ScopeGuard gltfMesh = json.beginObjectScope();
            json.writeKey("primitives"_s);

            /* Just a single primitive for each */
            {
                const Containers::ScopeGuard gltfPrimitives = json.beginArrayScope();
                const Containers::ScopeGuard gltfPrimitive = json.beginObjectScope();

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
                if(material != -1)
                    json.writeKey("material"_s).write(material);
            }

            if(mesh.gltfName)
                json.writeKey("name"_s).write(mesh.gltfName);
        };

        Containers::BitArray referencedMeshes{DirectInit, _state->meshes.size(), false};
        for(const Containers::Pair<UnsignedInt, Int> meshMaterialAssignment: _state->meshMaterialAssignments) {
            referencedMeshes.set(meshMaterialAssignment.first());

            writeMesh(json, _state->meshes[meshMaterialAssignment.first()], meshMaterialAssignment.second());
        }

        for(std::size_t i = 0; i != _state->meshes.size(); ++i) {
            if(referencedMeshes[i]) continue;

            writeMesh(json, _state->meshes[i], -1);
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

    /* Reserve the output array and write headers for a binary glTF */
    Containers::Array<char> out;
    if(_state->binary) {
        const std::size_t totalSize = 12 + /* file header */
            8 + json.size() + /* JSON chunk + header */
            (_state->buffer.isEmpty() ? 0 :
                8 + _state->buffer.size()); /* BIN chunk + header */
        Containers::arrayReserve<ArrayAllocator>(out, totalSize);

        /* glTF header */
        Containers::arrayAppend<ArrayAllocator>(out,
            "glTF\x02\x00\x00\x00"_s);
        Containers::arrayAppend<ArrayAllocator>(out,
            CharCaster{UnsignedInt(totalSize)}.data);

        /* JSON chunk header */
        Containers::arrayAppend<ArrayAllocator>(out,
            CharCaster{UnsignedInt(json.size())}.data);
        Containers::arrayAppend<ArrayAllocator>(out,
            "JSON"_s);

    /* Otherwise reserve just for the JSON */
    } else Containers::arrayReserve<ArrayAllocator>(out, json.size());

    /* Copy the JSON data to the output. In case of a text glTF we would
       ideally just pass the memory from the JsonWriter but the class uses an
       arbitrary growable deleter internally and custom deleters are forbidden
       in plugins. */
    /** @todo make it possible to specify an external allocator in JsonWriter
        once allocators-as-arguments are a thing */
    Containers::arrayAppend<ArrayAllocator>(out, json.toString());

    /* Add the buffer as a second BIN chunk for a binary glTF */
    if(_state->binary && !_state->buffer.isEmpty()) {
        Containers::arrayAppend<ArrayAllocator>(out,
            CharCaster{UnsignedInt(_state->buffer.size())}.data);
        Containers::arrayAppend<ArrayAllocator>(out,
            "BIN\0"_s);
        Containers::arrayAppend<ArrayAllocator>(out,
            _state->buffer);
    }

    /* GCC 4.8 and Clang 3.8 need extra help here */
    return Containers::optional(std::move(out));
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

void GltfSceneConverter::doSetSceneFieldName(const UnsignedInt field, const Containers::StringView name) {
    _state->sceneFieldNames[field] = Containers::String::nullTerminatedGlobalView(name);
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
                Error{} << "Trade::GltfSceneConverter::add(): scene parent mapping" << object << "out of bounds for" << scene.mappingBound() << "objects";
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
                    Error{} << "Trade::GltfSceneConverter::add(): scene parent reference" << parent << "out of bounds for" << scene.mappingBound() << "objects";
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
            if(!hasParent[i]) continue;
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
        const SceneField name = scene.fieldName(i);

        /* Skip fields that are treated differently */
        if(
            /* Parents are converted to a child list instead -- a presence of
               a parent field doesn't say anything about given object having
               any children */
            name == SceneField::Parent ||
            /* Materials are tied to the Mesh field -- if Mesh exists,
               Materials have the exact same mapping, thus there's no point in
               counting them separately */
            name == SceneField::MeshMaterial
        ) continue;

        /* Custom fields */
        if(isSceneFieldCustom(name)) {
            /* Skip ones for which we don't have a name */
            const auto found = _state->sceneFieldNames.find(sceneFieldCustom(name));
            if(found == _state->sceneFieldNames.end()) {
                Warning{} << "Trade::GltfSceneConverter::add(): custom scene field" << sceneFieldCustom(name) << "has no name assigned, skipping";
                continue;
            }

            /* Allow only scalar numbers for now */
            /** @todo For vectors / matrices it would be about `+= size`
                instead of `++objectFieldOffsets` below */
            const SceneFieldType type = scene.fieldType(i);
            if(type != SceneFieldType::UnsignedInt &&
               type != SceneFieldType::Int &&
               type != SceneFieldType::Float) {
                Warning{} << "Trade::GltfSceneConverter::add(): custom scene field" << found->second << "has unsupported type" << type << Debug::nospace << ", skipping";
                continue;
            }
        }

        usedFields.set(i);

        const Containers::ArrayView<UnsignedInt> mapping = mappingStorage.prefix(scene.fieldSize(i));
        scene.mappingInto(i, mapping);
        for(const UnsignedInt object: mapping) {
            if(object >= scene.mappingBound()) {
                Error{} << "Trade::GltfSceneConverter::add():" << scene.fieldName(i) << "mapping" << object << "out of bounds for" << scene.mappingBound() << "objects";
                return {};
            }

            /* Mark that the object has data. Will be used later to warn about
               objects that contained data but had no parents and thus were
               unused. */
            hasData.set(object);

            /* Objects that have no parent field are not exported thus their
               fields don't need to be counted either */
            if(!hasParent[object]) continue;

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
    for(UnsignedInt i = 0; i != scene.fieldCount(); ++i) {
        if(!usedFields[i]) continue;

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

        if(isSceneFieldCustom(fieldName))
            customFieldCount += size;
        else Warning{} << "Trade::GltfSceneConverter::add():" << scene.fieldName(i) << "was not used";
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
        it's enough to just take more items at once. Smaller types such as
        bools could fit into the 32-bit but strings would need a separate
        storage. */
    Containers::ArrayView<UnsignedInt> customFieldsUnsignedInt;
    Containers::ArrayTuple fieldStorage{
        {NoInit, totalFieldCount, fieldIds},
        {NoInit, totalFieldCount, fieldOffsets},
        {NoInit, transformationCount, transformations},
        {NoInit, hasTranslation ? trsCount : 0, translations},
        {NoInit, hasRotation ? trsCount : 0, rotations},
        {NoInit, hasScaling ? trsCount : 0, scalings},
        {NoInit, meshMaterialCount, meshesMaterials},
        {ValueInit, std::size_t(scene.mappingBound()), hasTrs},
        {NoInit, customFieldCount, customFieldsUnsignedInt}
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
            if(!hasParent[object]) continue;

            std::size_t& objectFieldOffset = objectFieldOffsets[object + 1];
            fieldIds[objectFieldOffset] = i;
            fieldOffsets[objectFieldOffset] = j;
            ++objectFieldOffset;
        }
    } {
        std::size_t offset = 0;
        for(UnsignedInt i = 0, iMax = scene.fieldCount(); i != iMax; ++i) {
            /* Only custom fields here, this means they're always last and all
               together, which makes it possible to write the "extras" object
               in one run. */
            if(!usedFields[i] || !isSceneFieldCustom(scene.fieldName(i)))
                continue;

            const std::size_t fieldSize = scene.fieldSize(i);
            const Containers::ArrayView<UnsignedInt> mapping = mappingStorage.prefix(fieldSize);
            scene.mappingInto(i, mapping);
            for(std::size_t j = 0; j != fieldSize; ++j) {
                const UnsignedInt object = mapping[j];

                /* Objects that have no parent field are not exported thus
                   their fields don't need to be counted either */
                if(!hasParent[object]) continue;

                std::size_t& objectFieldOffset = objectFieldOffsets[object + 1];
                fieldIds[objectFieldOffset] = i;
                /* As we put all custom fields into a single array, the offset
                   needs to also include sizes of all previous custom fields
                   already written. */
                /** @todo Currently abusing the fact that all whitelisted types
                    are numeric and 32bit. Once types of other sizes or string
                    / etc. fields are supported, there needs to be one offset
                    per type. */
                fieldOffsets[objectFieldOffset] = offset + j;
                ++objectFieldOffset;
            }

            offset += fieldSize;
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
        for(std::size_t i = 0; i != scene.fieldCount(); ++i) {
            if(!usedFields[i] || !isSceneFieldCustom(scene.fieldName(i)))
                continue;

            /** @todo this could be easily extended for 8- and 16-bit values,
                just Math::cast()'ing them to the output */
            const SceneFieldType type = scene.fieldType(i);
            const std::size_t size = scene.fieldSize(i);
            if(type == SceneFieldType::UnsignedInt) Utility::copy(
                scene.field<UnsignedInt>(i),
                customFieldsUnsignedInt.slice(offset, offset + size));
            else if(type == SceneFieldType::Int) Utility::copy(
                scene.field<Int>(i),
                customFieldsInt.slice(offset, offset + size));
            else if(type == SceneFieldType::Float) Utility::copy(
                scene.field<Float>(i),
                customFieldsFloat.slice(offset, offset + size));
            else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

            offset += size;
        }

        CORRADE_INTERNAL_ASSERT(offset == customFieldCount);
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
            if(hasData[object])
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

        SceneField previous{};
        for(std::size_t i = objectFieldOffsets[object], iMax = objectFieldOffsets[object + 1]; i != iMax; ++i) {
            const std::size_t offset = fieldOffsets[i];
            const SceneField fieldName = scene.fieldName(fieldIds[i]);
            if(fieldName == previous) {
                /** @todo special-case meshes (make multi-primitive meshes) */
                Warning w;
                w << "Trade::GltfSceneConverter::add(): ignoring duplicate field";
                if(isSceneFieldCustom(fieldName)) {
                    const auto found = _state->sceneFieldNames.find(sceneFieldCustom(fieldName));
                    CORRADE_INTERNAL_ASSERT(found != _state->sceneFieldNames.end());
                    w << found->second;
                } else w << previous;
                w << "for object" << object;
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

                const SceneFieldType type = scene.fieldType(fieldIds[i]);
                if(type == SceneFieldType::UnsignedInt)
                    _state->gltfNodes.write(customFieldsUnsignedInt[offset]);
                else if(type == SceneFieldType::Int)
                    _state->gltfNodes.write(customFieldsInt[offset]);
                else if(type == SceneFieldType::Float)
                    _state->gltfNodes.write(customFieldsFloat[offset]);
                else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

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
                Containers::Optional<UnsignedInt> meshId;
                for(UnsignedInt j = 0; j != _state->meshMaterialAssignments.size(); ++j) {
                    /** @todo something better than O(n^2) lookup! */
                    if(_state->meshMaterialAssignments[j] == meshesMaterials[offset]) {
                        meshId = j;
                        break;
                    }
                }
                if(!meshId) {
                    meshId = _state->meshMaterialAssignments.size();
                    arrayAppend(_state->meshMaterialAssignments, meshesMaterials[offset]);
                }
                _state->gltfNodes.writeKey("mesh"_s).write(*meshId);

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

        if(extrasOpen) _state->gltfNodes.endObject();

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

void GltfSceneConverter::doSetMeshAttributeName(const UnsignedShort attribute, Containers::StringView name) {
    /* Replace the previous entry if already set */
    for(Containers::Pair<UnsignedShort, Containers::String>& i: _state->customMeshAttributes) {
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

    /* Check and convert mesh index type */
    Int gltfIndexType;
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
        } else Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing an attribute-less mesh";

    /* 3.7.2.1 (Geometry § Meshes § Overview) says "[count] MUST be non-zero";
       we allow this in non-strict mode. Attribute-less meshes in glTF
       implicitly have zero vertices, so don't warn twice in that case. */
    } else if(!mesh.vertexCount()) {
        if(configuration().value<bool>("strict")) {
            Error{} << "Trade::GltfSceneConverter::add(): meshes with zero vertices are not valid glTF, set strict=false to allow them";
            return {};
        } else Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing a mesh with zero vertices";
    }

    /* Check and convert attributes */
    /** @todo detect and merge interleaved attributes into common buffer views */
    Containers::Array<Containers::Triple<Containers::String, Containers::StringView, Int>> gltfAttributeNamesTypes;
    for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i) {
        arrayAppend(gltfAttributeNamesTypes, InPlaceInit);

        /** @todo option to skip unrepresentable attributes instead of failing
            the whole mesh */

        const VertexFormat format = mesh.attributeFormat(i);
        if(isVertexFormatImplementationSpecific(format)) {
            Error{} << "Trade::GltfSceneConverter::add(): implementation-specific vertex format" << reinterpret_cast<void*>(vertexFormatUnwrap(format)) << "can't be exported";
            return {};
        }

        const UnsignedInt componentCount = vertexFormatComponentCount(format);
        const UnsignedInt vectorCount = vertexFormatVectorCount(format);
        const MeshAttribute attributeName = mesh.attributeName(i);

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

            if(format != VertexFormat::Vector3 &&
               format != VertexFormat::Vector4 &&
               format != VertexFormat::Vector3ubNormalized &&
               format != VertexFormat::Vector4ubNormalized &&
               format != VertexFormat::Vector3usNormalized &&
               format != VertexFormat::Vector4usNormalized) {
                Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh color attribute format" << format;
                return {};
            }

        /* Otherwise it's a custom attribute where anything representable by
           glTF is allowed */
        } else {
            switch(attributeName) {
                /* LCOV_EXCL_START */
                case MeshAttribute::Position:
                case MeshAttribute::Normal:
                case MeshAttribute::TextureCoordinates:
                case MeshAttribute::Color:
                    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
                /* LCOV_EXCL_STOP */

                case MeshAttribute::Tangent:
                    CORRADE_INTERNAL_ASSERT(componentCount == 3);
                    gltfAttributeName = Containers::String::nullTerminatedGlobalView("_TANGENT3"_s);
                    Warning{} << "Trade::GltfSceneConverter::add(): exporting three-component mesh tangents as a custom" << gltfAttributeName << "attribute";
                    break;

                case MeshAttribute::Bitangent:
                    gltfAttributeName = Containers::String::nullTerminatedGlobalView("_BITANGENT"_s);
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
                const UnsignedInt customAttributeId = meshAttributeCustom(attributeName);
                for(const Containers::Pair<UnsignedShort, Containers::String>& j: _state->customMeshAttributes) {
                    if(j.first() == customAttributeId) {
                        /* Make a non-owning reference to avoid a copy */
                        gltfAttributeName = Containers::String::nullTerminatedView(j.second());
                        break;
                    }
                }
                if(!gltfAttributeName) {
                    gltfAttributeName = Utility::format("_{}", meshAttributeCustom(attributeName));
                    Warning{} << "Trade::GltfSceneConverter::add(): no name set for" << attributeName << Debug::nospace << ", exporting as" << gltfAttributeName;
                }
            }
        }

        /** @todo spec says that POSITION accessor MUST have its min and max
            properties defined, I don't care at the moment */

        /* If a builtin glTF numbered attribute, append an ID to the name */
        if(gltfAttributeName == "TEXCOORD"_s ||
           gltfAttributeName == "COLOR"_s ||
           /* Not a builtin MeshAttribute yet, but expected to be used by
              people until builtin support is added */
           gltfAttributeName == "JOINTS"_s ||
           gltfAttributeName == "WEIGHTS"_s)
        {
            gltfAttributeName = Utility::format("{}_{}", gltfAttributeName, mesh.attributeId(i));

        /* Otherwise, if it's a second or further duplicate attribute,
           underscore it if not already and append an ID as well -- e.g. second
           and third POSITION attribute becomes _POSITION_1 and _POSITION_2,
           secondary _OBJECT_ID becomes _OBJECT_ID_1 */
        } else if(const UnsignedInt attributeId = mesh.attributeId(i)) {
            gltfAttributeName = Utility::format(
                gltfAttributeName.hasPrefix('_') ? "{}_{}" : "_{}_{}",
                gltfAttributeName, attributeId);
        }

        Containers::StringView gltfAccessorType;
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

        /* glTF requires matrices to be aligned to four bytes -- i.e., using
           the Matrix2x2bNormalizedAligned, Matrix3x3bNormalizedAligned or Matrix3x3sNormalizedAligned formats instead of the formats missing
           the Aligned suffix. Fortunately we don't need to check each
           individually as we have a neat tool instead. */
        if(vectorCount != 1 && vertexFormatVectorStride(format) % 4 != 0) {
            Error{} << "Trade::GltfSceneConverter::add(): mesh matrix attributes are required to be four-byte-aligned but got" << format;
            return {};
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
            } else Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing a 32-bit integer attribute" << gltfAttributeName;

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
        if(mesh.attributeArraySize(i) != 0) {
            Error{} << "Trade::GltfSceneConverter::add(): unsupported mesh attribute with array size" << mesh.attributeArraySize(i);
            return {};
        }

        gltfAttributeNamesTypes.back() = {std::move(gltfAttributeName), gltfAccessorType, gltfAccessorComponentType};
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
                .writeKey("byteLength"_s).write(indexData.size());
            /** @todo target, once we don't have one view per accessor */
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

        /* Vertex data */
        Containers::ArrayView<char> vertexData = arrayAppend(_state->buffer, mesh.vertexData());

        /* Attribute views and accessors */
        for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i) {
            const VertexFormat format = mesh.attributeFormat(i);

            /* Flip texture coordinates unless they're meant to be flipped in
               the material */
            if(mesh.attributeName(i) == MeshAttribute::TextureCoordinates && !configuration().value<bool>("textureCoordinateYFlipInMaterial")) {
                Containers::StridedArrayView1D<char> data{vertexData,
                    vertexData + mesh.attributeOffset(i),
                    mesh.vertexCount(), mesh.attributeStride(i)};
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

            const std::size_t formatSize = vertexFormatSize(format);
            const std::size_t attributeStride = mesh.attributeStride(i);
            const std::size_t gltfBufferViewIndex = _state->gltfBufferViews.currentArraySize();
            const Containers::ScopeGuard gltfBufferView = _state->gltfBufferViews.beginObjectScope();
            _state->gltfBufferViews
                .writeKey("buffer"_s).write(0)
                /* Byte offset could be omitted if zero but since that
                   happens only for the very first view in a buffer and we
                   have always at most one buffer, the minimal savings are
                   not worth the inconsistency */
                .writeKey("byteOffset"_s).write(vertexData - _state->buffer + mesh.attributeOffset(i));

            /* Byte length, make sure to not count padding into it as that'd
               fail bound checks. If there are no vertices, the length is
               zero. */
            /** @todo spec says it can't be smaller than stride (for
                single-vertex meshes), fix alongside merging buffer views for
                interleaved attributes */
            const std::size_t gltfByteLength = mesh.vertexCount() ?
                /** @todo this needs to include array size once we use that for
                    builtin attributes (skinning?) */
                attributeStride*(mesh.vertexCount() - 1) + formatSize : 0;
            _state->gltfBufferViews.writeKey("byteLength"_s).write(gltfByteLength);

            /* If byteStride is omitted, it's implicitly treated as tightly
               packed, same as in GL. If/once views get shared, this needs to
               also check that the view isn't shared among multiple
               accessors. */
            if(attributeStride != formatSize)
                _state->gltfBufferViews.writeKey("byteStride"_s).write(attributeStride);

            /** @todo target, once we don't have one view per accessor */

            if(configuration().value<bool>("accessorNames"))
                _state->gltfBufferViews.writeKey("name"_s).write(Utility::format(
                    name ? "mesh {0} ({1}) {2}" : "mesh {0} {2}",
                    id, name, gltfAttributeNamesTypes[i].first()));

            const UnsignedInt gltfAccessorIndex = _state->gltfAccessors.currentArraySize();
            const Containers::ScopeGuard gltfAccessor = _state->gltfAccessors.beginObjectScope();
            _state->gltfAccessors
                .writeKey("bufferView"_s).write(gltfBufferViewIndex)
                /* We don't share views among accessors yet, so bufferOffset is
                   implicitly 0 */
                .writeKey("componentType"_s).write(gltfAttributeNamesTypes[i].third());
            if(isVertexFormatNormalized(format))
                _state->gltfAccessors.writeKey("normalized"_s).write(true);
            _state->gltfAccessors
                .writeKey("count"_s).write(mesh.vertexCount())
                .writeKey("type"_s).write(gltfAttributeNamesTypes[i].second());
            if(configuration().value<bool>("accessorNames"))
                _state->gltfAccessors.writeKey("name"_s).write(Utility::format(
                    name ? "mesh {0} ({1}) {2}" : "mesh {0} {2}",
                    id, name, gltfAttributeNamesTypes[i].first()));

            arrayAppend(meshProperties.gltfAttributes, InPlaceInit, gltfAttributeNamesTypes[i].first(), gltfAccessorIndex);
        }

        /* Triangles are a default */
        if(gltfMode != 4) meshProperties.gltfMode = gltfMode;
    }

    if(name) meshProperties.gltfName = name;

    return true;
}

namespace {

/* Remembers which attributes were accessed to subsequently handle ones that
   weren't */
struct MaskedMaterial {
    explicit MaskedMaterial(const MaterialData& material, UnsignedInt layer = 0): material(material), layer{layer}, mask{ValueInit, material.attributeCount(layer)} {}

    Containers::Optional<UnsignedInt> findId(MaterialAttribute name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(layer, name);
        if(!found) return {};

        mask.set(*found);
        return found;
    }

    template<class T> Containers::Optional<T> find(Containers::StringView name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(layer, name);
        if(!found) return {};

        mask.set(*found);
        return material.attribute<T>(layer, *found);
    }

    template<class T> Containers::Optional<T> find(MaterialAttribute name) {
        const Containers::Optional<UnsignedInt> found = material.findAttributeId(layer, name);
        if(!found) return {};

        mask.set(*found);
        return material.attribute<T>(layer, *found);
    }

    const MaterialData& material;
    UnsignedInt layer;
    Containers::BitArray mask;
};

}

bool GltfSceneConverter::doAdd(UnsignedInt, const MaterialData& material, const Containers::StringView name) {
    const auto& pbrMetallicRoughnessMaterial = material.as<PbrMetallicRoughnessMaterialData>();

    /* Check that all referenced textures are in bounds */
    for(const MaterialAttribute attribute: {
        MaterialAttribute::BaseColorTexture,
        MaterialAttribute::MetalnessTexture,
        MaterialAttribute::RoughnessTexture,
        MaterialAttribute::NormalTexture,
        MaterialAttribute::OcclusionTexture,
        MaterialAttribute::EmissiveTexture
    }) {
        const Containers::Optional<UnsignedInt> id = material.findAttributeId(attribute);
        if(!id) continue;

        const UnsignedInt index = material.attribute<UnsignedInt>(*id);
        if(index >= textureCount()) {
            Error{} << "Trade::GltfSceneConverter::add(): material attribute" << material.attributeName(*id) << "references texture" << index << "but only" << textureCount() << "were added so far";
            return {};
        }

        /* If there's a layer, validate that it's in bounds as well. For 2D
           textures the layer count is implicitly 1, so the layer can only be
           0. */
        CORRADE_INTERNAL_ASSERT(textureCount() + 1 == _state->textureIdOffsets.size());
        const Containers::String layerAttributeName = materialAttributeName(attribute) + "Layer"_s;
        if(const Containers::Optional<UnsignedInt> layer = material.findAttribute<UnsignedInt>(layerAttributeName)) {
            const UnsignedInt textureLayerCount = _state->textureIdOffsets[index + 1] - _state->textureIdOffsets[index];
            if(*layer >= textureLayerCount) {
                Error{} << "Trade::GltfSceneConverter::add(): material attribute" << layerAttributeName << "value" << *layer << "out of range for" << textureLayerCount << "layers in texture" << index;
                return {};
            }
        }
    }

    /* Check that all textures are using a compatible packing */
    if(pbrMetallicRoughnessMaterial.hasMetalnessTexture() != pbrMetallicRoughnessMaterial.hasRoughnessTexture()) {
        /** @todo turn this into a warning and ignore the lone texture in that
            case? */
        Error{} << "Trade::GltfSceneConverter::add(): can only represent a combined metallic/roughness texture or neither of them";
        return {};
    }
    if(pbrMetallicRoughnessMaterial.hasMetalnessTexture() && pbrMetallicRoughnessMaterial.hasRoughnessTexture() && !pbrMetallicRoughnessMaterial.hasNoneRoughnessMetallicTexture()) {
        /** @todo this message is confusing if swizzle is alright but e.g.
            Matrix or Coordinates are different */
        Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrMetallicRoughnessMaterial.metalnessTextureSwizzle() << Debug::nospace << "/" << Debug::nospace << Debug::packed << pbrMetallicRoughnessMaterial.roughnessTextureSwizzle() << "packing of a metallic/roughness texture";
        return {};
    }
    if(material.hasAttribute(MaterialAttribute::NormalTexture) && pbrMetallicRoughnessMaterial.normalTextureSwizzle() != MaterialTextureSwizzle::RGB) {
        Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrMetallicRoughnessMaterial.normalTextureSwizzle() << "packing of a normal texture";
        return {};
    }
    if(material.hasAttribute(MaterialAttribute::OcclusionTexture) && pbrMetallicRoughnessMaterial.occlusionTextureSwizzle() != MaterialTextureSwizzle::R) {
        Error{} << "Trade::GltfSceneConverter::add(): unsupported" << Debug::packed << pbrMetallicRoughnessMaterial.occlusionTextureSwizzle() << "packing of an occlusion texture";
        return {};
    }

    /* At this point we're sure nothing will fail so we can start writing the
       JSON. Otherwise we'd end up with a partly-written JSON in case of an
       unsupported mesh, corruputing the output. */

    /* If this is a first material, open the materials array */
    if(_state->gltfMaterials.isEmpty())
        _state->gltfMaterials.beginArray();

    const Containers::ScopeGuard gltfMaterial = _state->gltfMaterials.beginObjectScope();

    const bool keepDefaults = configuration().value<bool>("keepMaterialDefaults");

    auto writeTextureContents = [&](MaskedMaterial& maskedMaterial, UnsignedInt textureAttributeId, Containers::StringView prefix) {
        if(!prefix) prefix = maskedMaterial.material.attributeName(textureAttributeId);

        /* Bounds of all textures should have been verified at the very top */
        const UnsignedInt texture = maskedMaterial.material.attribute<UnsignedInt>(textureAttributeId);
        CORRADE_INTERNAL_ASSERT(texture < _state->textureIdOffsets.size());

        /* Texture layer. If there's no such attribute, it's implicitly 0.
           Layer index bounds should have been verified at the very top as
           well. */
        Containers::Optional<UnsignedInt> layer = maskedMaterial.find<UnsignedInt>(prefix + "Layer"_s);
        if(!layer)
            layer = maskedMaterial.find<UnsignedInt>(MaterialAttribute::TextureLayer);
        if(!layer)
            layer = 0;
        CORRADE_INTERNAL_ASSERT(*layer < _state->textureIdOffsets[texture + 1] - _state->textureIdOffsets[texture]);

        _state->gltfMaterials.writeKey("index"_s).write(_state->textureIdOffsets[texture] + *layer);

        Containers::Optional<UnsignedInt> textureCoordinates = maskedMaterial.find<UnsignedInt>(prefix + "Coordinates"_s);
        if(!textureCoordinates)
            textureCoordinates = maskedMaterial.find<UnsignedInt>(MaterialAttribute::TextureCoordinates);
        if(textureCoordinates && (keepDefaults || *textureCoordinates != 0))
            _state->gltfMaterials.writeKey("texCoord"_s).write(*textureCoordinates);

        Containers::String textureMatrixAttribute = prefix + "Matrix"_s;
        Containers::Optional<Matrix3> textureMatrix = maskedMaterial.find<Matrix3>(textureMatrixAttribute);
        if(!textureMatrix) {
            textureMatrixAttribute = materialAttributeName(MaterialAttribute::TextureMatrix);
            textureMatrix = maskedMaterial.find<Matrix3>(textureMatrixAttribute);
        }

        /* If there's no matrix but we're told to Y-flip texture coordinates in
           the material, add an identity -- down below it'll be converted to an
           Y-flipping one */
        if(!textureMatrix && configuration().value<bool>("textureCoordinateYFlipInMaterial"))
            textureMatrix.emplace();

        if(textureMatrix) {
            /* Arbitrary rotation not supported yet, as there's several
               equivalent decompositions for an arbitrary matrix and I'm too
               lazy to try to find the most minimal one each time. This way I
               can also get away with just reusing the diagonal signs for
               scaling. */
            const Matrix3 exceptRotation = Matrix3::translation(textureMatrix->translation())*Matrix3::scaling(textureMatrix->scaling()*Math::sign(textureMatrix->diagonal().xy()));
            if(exceptRotation != *textureMatrix) {
                Warning{} << "Trade::GltfSceneConverter::add(): material attribute" << textureMatrixAttribute << "rotation was not used";
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
        }
    };
    auto writeTexture = [&](MaskedMaterial& maskedMaterial, Containers::StringView name, UnsignedInt textureAttributeId, Containers::StringView prefix) {
        _state->gltfMaterials.writeKey(name);
        const Containers::ScopeGuard gltfTexture = _state->gltfMaterials.beginObjectScope();

        writeTextureContents(maskedMaterial, textureAttributeId, prefix);
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
    MaskedMaterial maskedMaterial{material};

    /* Metallic/roughness material properties. Write only if there's actually
       something; texture properties will get ignored if there's no texture. */
    {
        const auto baseColor = maskedMaterial.find<Color4>(MaterialAttribute::BaseColor);
        const auto metalness = maskedMaterial.find<Float>(MaterialAttribute::Metalness);
        const auto roughness = maskedMaterial.find<Float>(MaterialAttribute::Roughness);
        const auto foundBaseColorTexture = maskedMaterial.findId(MaterialAttribute::BaseColorTexture);
        /* It was checked above that the correct Metallic/Roughness packing
           is used, so we can check either just for the metalness texture or
           for the combined one -- the roughness texture attributes are then
           exactly the same */
        const auto foundMetalnessTexture = maskedMaterial.findId(MaterialAttribute::MetalnessTexture);
        const auto foundNoneRoughnessMetallicTexture = maskedMaterial.findId(MaterialAttribute::NoneRoughnessMetallicTexture);
        if((baseColor && (keepDefaults || *baseColor != 0xffffffff_rgbaf)) ||
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
                writeTexture(maskedMaterial, "baseColorTexture"_s, *foundBaseColorTexture, {});

            if(metalness && (keepDefaults || Math::notEqual(*metalness, 1.0f)))
                _state->gltfMaterials
                    .writeKey("metallicFactor"_s).write(*metalness);
            if(roughness && (keepDefaults || Math::notEqual(*roughness, 1.0f)))
                _state->gltfMaterials
                    .writeKey("roughnessFactor"_s).write(*roughness);
            if(foundMetalnessTexture) {
                writeTexture(maskedMaterial, "metallicRoughnessTexture"_s, *foundMetalnessTexture, {});

                /* Mark the swizzles and roughness properties as used, if
                   present, by simply looking them up -- we checked they're
                   valid and consistent with metalness above */
                maskedMaterial.findId(MaterialAttribute::MetalnessTextureSwizzle);
                maskedMaterial.findId(MaterialAttribute::RoughnessTexture);
                maskedMaterial.findId(MaterialAttribute::RoughnessTextureSwizzle);
                maskedMaterial.findId(MaterialAttribute::RoughnessTextureMatrix);
                maskedMaterial.findId(MaterialAttribute::RoughnessTextureCoordinates);
                maskedMaterial.findId(MaterialAttribute::RoughnessTextureLayer);

            } else if(foundNoneRoughnessMetallicTexture) {
                writeTexture(maskedMaterial, "metallicRoughnessTexture"_s, *foundNoneRoughnessMetallicTexture, "MetalnessTexture"_s);

                /* Mark the roughness properties as used, if present, by simply
                   looking them up -- we checked they're consistent with
                   metalness above */
                maskedMaterial.findId(MaterialAttribute::RoughnessTextureMatrix);
                maskedMaterial.findId(MaterialAttribute::RoughnessTextureCoordinates);
                maskedMaterial.findId(MaterialAttribute::RoughnessTextureLayer);
            }
        }
    }

    /* Normal texture properties; ignored if there's no texture */
    if(const auto foundNormalTexture = maskedMaterial.findId(MaterialAttribute::NormalTexture)) {
        _state->gltfMaterials.writeKey("normalTexture"_s);
        const Containers::ScopeGuard gltfTexture = _state->gltfMaterials.beginObjectScope();

        writeTextureContents(maskedMaterial, *foundNormalTexture, {});

        /* Mark the swizzle as used, if present, by simply looking it up -- we
           checked it's valid above */
        maskedMaterial.findId(MaterialAttribute::NormalTextureSwizzle);

        const auto normalTextureScale = maskedMaterial.find<Float>(MaterialAttribute::NormalTextureScale);
        if(normalTextureScale && (keepDefaults || Math::notEqual(*normalTextureScale, 1.0f)))
            _state->gltfMaterials
                .writeKey("scale"_s).write(*normalTextureScale);
    }

    /* Occlusion texture properties; ignored if there's no texture */
    if(const auto foundOcclusionTexture = maskedMaterial.findId(MaterialAttribute::OcclusionTexture)) {
        _state->gltfMaterials.writeKey("occlusionTexture"_s);
        const Containers::ScopeGuard gltfTexture = _state->gltfMaterials.beginObjectScope();

        writeTextureContents(maskedMaterial, *foundOcclusionTexture, {});

        /* Mark the swizzle as used, if present, by simply looking it up -- we
           checked it's valid above */
        maskedMaterial.findId(MaterialAttribute::OcclusionTextureSwizzle);

        const auto occlusionTextureStrength = maskedMaterial.find<Float>(MaterialAttribute::OcclusionTextureStrength);
        if(occlusionTextureStrength && (keepDefaults || Math::notEqual(*occlusionTextureStrength, 1.0f)))
            _state->gltfMaterials
                .writeKey("strength"_s).write(*occlusionTextureStrength);
    }

    /* Emissive factor */
    {
        const auto emissiveColor = maskedMaterial.find<Color3>(MaterialAttribute::EmissiveColor);
        if(emissiveColor && (keepDefaults || *emissiveColor != 0x000000_rgbf))
            _state->gltfMaterials
                .writeKey("emissiveFactor"_s).writeArray(emissiveColor->data());
    }

    /* Emissive texture properties; ignored if there's no texture */
    if(const auto foundEmissiveTexture = maskedMaterial.findId(MaterialAttribute::EmissiveTexture))
        writeTexture(maskedMaterial, "emissiveTexture"_s, *foundEmissiveTexture, {});

    /* Alpha mode and cutoff */
    {
        const auto alphaMask = maskedMaterial.find<Float>(MaterialAttribute::AlphaMask);
        const auto alphaBlend = maskedMaterial.find<bool>(MaterialAttribute::AlphaBlend);
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
        const auto doubleSided = maskedMaterial.find<bool>(MaterialAttribute::DoubleSided);
        if(doubleSided && (keepDefaults || *doubleSided))
            _state->gltfMaterials.writeKey("doubleSided"_s).write(*doubleSided);
    }

    /* Flat material */
    if(material.types() & MaterialType::Flat) {
        _state->usedExtensions |= GltfExtension::KhrMaterialsUnlit;
        _state->gltfMaterials.writeKey("extensions"_s)
            .beginObject()
                .writeKey("KHR_materials_unlit"_s).beginObject().endObject()
            .endObject();
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
            maskedMaterial.mask.set(*diffuseColorId);
    } {
        const auto baseColorTextureId = material.findAttributeId(MaterialAttribute::BaseColorTexture);
        const auto diffuseTextureId = material.findAttributeId(MaterialAttribute::DiffuseTexture);
        if(baseColorTextureId && diffuseTextureId && material.attribute<UnsignedInt>(*baseColorTextureId) == material.attribute<UnsignedInt>(*diffuseTextureId))
            maskedMaterial.mask.set(*diffuseTextureId);
    }  {
        const auto baseColorTextureMatrixId = material.findAttributeId(MaterialAttribute::BaseColorTextureMatrix);
        const auto diffuseTextureMatrixId = material.findAttributeId(MaterialAttribute::DiffuseTextureMatrix);
        if(baseColorTextureMatrixId && diffuseTextureMatrixId && material.attribute<Matrix3>(*baseColorTextureMatrixId) == material.attribute<Matrix3>(*diffuseTextureMatrixId))
            maskedMaterial.mask.set(*diffuseTextureMatrixId);
    } {
        const auto baseColorTextureCoordinatesId = material.findAttributeId(MaterialAttribute::BaseColorTextureCoordinates);
        const auto diffuseTextureCoordinatesId = material.findAttributeId(MaterialAttribute::DiffuseTextureCoordinates);
        if(baseColorTextureCoordinatesId && diffuseTextureCoordinatesId && material.attribute<UnsignedInt>(*baseColorTextureCoordinatesId) == material.attribute<UnsignedInt>(*diffuseTextureCoordinatesId))
            maskedMaterial.mask.set(*diffuseTextureCoordinatesId);
    } {
        const auto baseColorTextureLayerId = material.findAttributeId(MaterialAttribute::BaseColorTextureLayer);
        const auto diffuseTextureLayerId = material.findAttributeId(MaterialAttribute::DiffuseTextureLayer);
        if(baseColorTextureLayerId && diffuseTextureLayerId && material.attribute<UnsignedInt>(*baseColorTextureLayerId) == material.attribute<UnsignedInt>(*diffuseTextureLayerId))
            maskedMaterial.mask.set(*diffuseTextureLayerId);
    }

    /* Report unused attributes and layers */
    /** @todo some "iterate unset bits" API for this? */
    for(std::size_t i = 0; i != material.attributeCount(); ++i) {
        if(!maskedMaterial.mask[i])
            Warning{} << "Trade::GltfSceneConverter::add(): material attribute" << material.attributeName(i) << "was not used";
    }
    for(std::size_t i = 1; i != material.layerCount(); ++i) {
        /** @todo redo this once we actually use some layers */
        Warning w;
        w << "Trade::GltfSceneConverter::add(): material layer" << i;
        if(material.layerName(i))
            w << "(" << Debug::nospace << material.layerName(i) << Debug::nospace << ")";
        w << "was not used";
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

    /* Propagate configuration values */
    Utility::ConfigurationGroup& imageConverterConfiguration = imageConverter->configuration();
    for(const Containers::Pair<Containers::StringView, Containers::StringView> value: configuration.group("imageConverter")->values()) {
        if(!imageConverterConfiguration.hasValue(value.first()))
            Warning{} << "Trade::GltfSceneConverter::add(): option" << value.first() << "not recognized by" << plugin;

        imageConverterConfiguration.setValue(value.first(), value.second());
    }
    if(configuration.group("imageConverter")->hasGroups()) {
        /** @todo once image converters have groups, propagate that as well;
            then it might make sense to expose, test and reuse Magnum's own
            MagnumPlugins/Implementation/propagateConfiguration.h */
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
            .writeKey("uri"_s).write(Utility::Path::split(imageFilename).second());
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
    if(!imageConverter) return {};

    /* Use a MIME type to decide what glTF extension (if any) to use to
       reference the image from a texture. Could also use the file extension,
       but a MIME type is more robust and all image converter plugins except
       Basis Universal have it. */
    const Containers::String mimeType = imageConverter->mimeType();
    GltfExtension extension;
    if(mimeType == "image/jpeg"_s ||
       mimeType == "image/png"_s) {
        extension = GltfExtension{};
    /** @todo some more robust way to detect if Basis-encoded KTX image is
        produced? waiting until the image is produced and then parsing the
        header is insanely complicated :( */
    } else if(mimeType == "image/ktx2"_s && imageConverterPluginName == "BasisKtxImageConverter"_s) {
        extension = GltfExtension::KhrTextureBasisu;
    } else if(mimeType == "image/ktx2"_s && configuration().value<bool>("experimentalKhrTextureKtx")) {
        extension = GltfExtension::KhrTextureKtx;
    /** @todo EXT_texture_webp and MSFT_texture_dds, once we have converters */
    } else {
        if(!mimeType) {
            Error{} << "Trade::GltfSceneConverter::add():" << imageConverterPluginName << "doesn't specify any MIME type, can't save an image";
            return {};
        }

        if(mimeType == "image/ktx2"_s && !configuration().value<bool>("experimentalKhrTextureKtx"))
            Warning{} << "Trade::GltfSceneConverter::add(): KTX2 images can be saved using the KHR_texture_ktx extension, enable experimentalKhrTextureKtx to use it";

        if(configuration().value<bool>("strict")) {
            Error{} << "Trade::GltfSceneConverter::add():" << mimeType << "is not a valid MIME type for a glTF image, set strict=false to allow it";
            return {};
        } else Warning{} << "Trade::GltfSceneConverter::add(): strict mode disabled, allowing" << mimeType << "MIME type for an image";

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
    if(!imageConverter) return {};

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
    "cz.mosra.magnum.Trade.AbstractSceneConverter/0.2.1")
