/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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

#include "GltfImporter.h"

#include <algorithm> /* std::stable_sort() */
#include <cctype>
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayTuple.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Json.h>
#include <Corrade/Utility/MurmurHash2.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/CubicHermite.h>
#include <Magnum/Math/FunctionsBatch.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/SkinData.h>
#include <Magnum/Trade/TextureData.h>
#include <MagnumPlugins/AnyImageImporter/AnyImageImporter.h>

#include "MagnumPlugins/GltfImporter/decode.h"
#include "MagnumPlugins/GltfImporter/Gltf.h"

#ifdef CORRADE_MSVC2015_COMPATIBILITY
/* Otherwise std::unique() fails to compile on MSVC 2015. Other compilers are
   fine without. */
#include <Corrade/Containers/StridedArrayViewStl.h>
#endif

/* We'd have to endian-flip everything that comes from buffers, plus the binary
   glTF headers, etc. Too much work, hard to automatically test because the
   HW is hard to get. */
#ifdef CORRADE_TARGET_BIG_ENDIAN
#error this code will not work on Big Endian, sorry
#endif

/* std::hash specialization to be able to use StringView in unordered_map.
   Injecting this into namespace std seems to be the designated way but it
   feels wrong. */
namespace std {
    template<> struct hash<Corrade::Containers::StringView> {
        std::size_t operator()(const Corrade::Containers::StringView& key) const {
            const Corrade::Utility::MurmurHash2 hash;
            const Corrade::Utility::HashDigest<sizeof(std::size_t)> digest = hash(key.data(), key.size());
            return *reinterpret_cast<const std::size_t*>(digest.byteArray());
        }
    };
}

namespace Magnum { namespace Trade {

using namespace Containers::Literals;
using namespace Math::Literals;

namespace {

/* Data URI according to RFC 2397, used by loadUri() and
   setupOrReuseImporterForImage() */
inline bool isDataUri(const Containers::StringView uri) {
    return uri.hasPrefix("data:"_s);
}

/* Used by doOpenData() and doMesh() */
bool isBuiltinNumberedMeshAttribute(const Containers::StringView name) {
    const Containers::Array3<Containers::StringView> attributeNameNumber = name.partition('_');
    return
        (attributeNameNumber[0] == "TEXCOORD"_s ||
         attributeNameNumber[0] == "COLOR"_s ||
         /* Not a builtin MeshAttribute yet, but expected to be used by
            people until builtin support is added */
         attributeNameNumber[0] == "JOINTS"_s ||
         attributeNameNumber[0] == "WEIGHTS"_s) &&
        /* Assumes just a single number. glTF doesn't say anything about the
           upper limit, but for now it should be fine to allow 10 attributes
           at most. Thus TEXCOORD, TEXCOORD_SECOND or TEXCOORD_10 would fail
           this check. */
        /** @todo a more flexible parsing once we have our number parsers
            that don't rely on null-terminated strings */
        attributeNameNumber[2].size() == 1 && attributeNameNumber[2][0] >= '0' && attributeNameNumber[2][0] <= '9';
}

/* Used by doOpenData() */
bool isBuiltinMeshAttribute(Utility::ConfigurationGroup& configuration, const Containers::StringView name) {
    return
        name == "POSITION"_s ||
        name == "NORMAL"_s ||
        name == "TANGENT"_s ||
        name == "COLOR"_s ||
        name == configuration.value<Containers::StringView>("objectIdAttribute") ||
        isBuiltinNumberedMeshAttribute(name);
}

}

struct GltfImporter::Document {
    /* Set only if fromFile() was used, passed to Utility::Json for nicer error
       messages and used as a base path for buffer and image opening */
    Containers::Optional<Containers::String> filename;

    /* File data, to which point parsed glTF tokens and the BIN chunk, if
       present */
    Containers::Array<char> fileData;
    Containers::Optional<Utility::Json> gltf;
    Containers::Optional<Containers::ArrayView<const char>> binChunk;

    /* Constant-time access to glTF data and their names. All these are checked
       to be object tokens during the initial import. Buffers, buffer views,
       accessors and samplers have names defined as well but we don't provide
       access to those, so no point in saving them. */
    Containers::Array<Containers::Reference<const Utility::JsonToken>>
        gltfBuffers,
        gltfBufferViews,
        gltfAccessors,
        gltfSamplers;
    Containers::Array<Containers::Pair<Containers::Reference<const Utility::JsonToken>, Containers::StringView>>
        gltfNodes,
        gltfMeshes, /* plus gltfMeshPrimitiveMap below */
        gltfCameras,
        gltfLights,
        gltfAnimations,
        gltfSkins,
        gltfImages,
        gltfTextures,
        gltfMaterials,
        gltfScenes;

    /* Storage for buffer content. If a buffer is fetched from a file callback,
       it's a non-owning view. These are filled on demand. We don't check for
       duplicate URIs since that's incredibly unlikely and hard to get right,
       so the buffer id is used as the index. If a buffer failed to load, it'll
       stay a NullOpt, meaning the same failure message will be printed next
       time it's accessed. */
    Containers::Array<Containers::Optional<Containers::Array<char>>> buffers;
    /* Parsed and validated buffer views, second element is stride (or 0 if not
       strided), third is buffer ID. Same as with buffers, if any of these
       failed to validate, it'll stay a NullOpt, meaning the same failure
       message will be printed next time it's accessed. */
    Containers::Array<Containers::Optional<Containers::Triple<Containers::ArrayView<const char>, UnsignedInt, UnsignedInt>>> bufferViews;
    /* Parsed and validated buffer views, second element is the parsed type,
       third is buffer view ID. As the type is known, it's always a 2D view
       with layout as expected. Same as with buffers and buffer views, if any
       of these failed to validate, it'll stay a NullOpt, meaning the same
       failure message will be printed next time it's accessed.

       We're abusing VertexFormat here because it can describe all types
       supported by glTF including aligned matrices and because there's a
       builtin way to create a composite type out of component type,
       component/vector count and the normalized bit. Error messages print it
       without the VertexFormat:: prefix to avoid confusion, yet I think saying
       something like "Vector3ubNormalized is not a supported normal format" is
       better than "normalized VEC3 of 5121 is not a supported normal format"
       no matter how well formatted. */
    Containers::Array<Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>>> accessors;
    /* Cached parsed samplers. Values left uninitialized, they will be set to
       appropriate default values inside doTexture(). */
    struct Sampler {
        SamplerFilter minificationFilter;
        SamplerFilter magnificationFilter;
        SamplerMipmap mipmap;
        Math::Vector3<SamplerWrapping> wrapping;
    };
    Containers::Array<Containers::Optional<Sampler>> samplers;

    /* We can use StringView as the map key here because all views point to
       strings stored inside Utility::Json which ensures the pointers are
       stable and won't go out of scope. */
    Containers::Optional<std::unordered_map<Containers::StringView, Int>>
        animationsForName,
        camerasForName,
        lightsForName,
        scenesForName,
        skinsForName,
        nodesForName,
        meshesForName,
        materialsForName,
        imagesForName,
        texturesForName;

    /* Unlike the ones above, these are filled already during construction as
       we need them in three different places and on-demand construction would
       be too annoying to test. */
    std::unordered_map<Containers::StringView, MeshAttribute>
        meshAttributesForName{
         /* Not a builtin MeshAttribute yet, but expected to be used by
            people until builtin support is added. Wouldn't strictly need to be
            present if the file has no skinning meshes but having them present
            in the map always makes the implementation simpler. */
        {"JOINTS"_s, meshAttributeCustom(0)},
        {"WEIGHTS"_s, meshAttributeCustom(1)}
    };
    Containers::Array<Containers::StringView> meshAttributeNames{InPlaceInit, {
        "JOINTS"_s,
        "WEIGHTS"_s
    }};

    /* Mapping for multi-primitive meshes:

        -   gltfMeshPrimitiveMap.size() is the count of meshes reported to the
            user
        -   meshSizeOffsets.size() is the count of original meshes in the file
        -   gltfMeshPrimitiveMap[id] is a pair of (original mesh ID, glTF
            primitive token); the primitive token is checked to be an object
            token during the initial import
        -   meshSizeOffsets[j] points to the first item in gltfMeshPrimitiveMap
            for original mesh ID `j` -- which also translates the original ID
            to reported ID
        -   meshSizeOffsets[j + 1] - meshSizeOffsets[j] is count of meshes for
            original mesh ID `j` (or number of primitives in given mesh)
    */
    Containers::Array<Containers::Pair<std::size_t, Containers::Reference<const Utility::JsonToken>>> gltfMeshPrimitiveMap;
    Containers::Array<std::size_t> meshSizeOffsets;

    /* If a file contains texture coordinates that are not floats or normalized
       in the 0-1, the textureCoordinateYFlipInMaterial option is enabled
       implicitly as we can't perform Y-flip directly on the data. */
    bool textureCoordinateYFlipInMaterial = false;

    UnsignedInt imageImporterId = ~UnsignedInt{};
    Containers::Optional<AnyImageImporter> imageImporter;
};

Containers::Optional<Containers::Array<char>> GltfImporter::loadUri(const char* const errorPrefix, const Containers::StringView uri) {
    if(isDataUri(uri)) {
        /* Data URI with base64 payload according to RFC 2397:
           data:[<mediatype>][;base64],<data> */
        Containers::StringView base64;
        const Containers::Array3<Containers::StringView> parts = uri.partition(',');

        /* Non-base64 data URIs are allowed by RFC 2397, but make no sense for
           glTF */
        if(parts.front().hasSuffix(";base64"_s)) {
            /* This will be empty for both a missing comma and an empty payload */
            base64 = parts.back();
        }

        if(base64.isEmpty()) {
            Error{} << errorPrefix << "data URI has no base64 payload";
            return Containers::NullOpt;
        }

        return decodeBase64(errorPrefix, base64);
    }

    const Containers::Optional<Containers::String> decodedUri = decodeUri(errorPrefix, uri);
    if(!decodedUri)
        return {};

    if(fileCallback()) {
        const Containers::String fullPath = Utility::Path::join(_d->filename ? Utility::Path::split(*_d->filename).first() : Containers::StringView{}, *decodedUri);
        if(Containers::Optional<Containers::ArrayView<const char>> view = fileCallback()(fullPath, InputFileCallbackPolicy::LoadPermanent, fileCallbackUserData()))
            /* Return a non-owning view */
            return Containers::Array<char>{const_cast<char*>(view->data()), view->size(), [](char*, std::size_t){}};

        Error{} << errorPrefix << "error opening" << fullPath << "through a file callback";
        return {};

    } else {
        if(!_d->filename) {
            Error{} << errorPrefix << "external buffers can be imported only when opening files from the filesystem or if a file callback is present";
            return Containers::NullOpt;
        }

        const Containers::String fullPath = Utility::Path::join(Utility::Path::split(*_d->filename).first(), *decodedUri);

        if(Containers::Optional<Containers::Array<char>> data = Utility::Path::read(fullPath))
            return data;

        Error{} << errorPrefix << "error opening" << fullPath;
        return {};
    }
}

Containers::Optional<Containers::ArrayView<const char>> GltfImporter::parseBuffer(const char* const errorPrefix, const UnsignedInt bufferId) {
    if(bufferId >= _d->gltfBuffers.size()) {
        Error{} << errorPrefix << "buffer index" << bufferId << "out of range for" << _d->gltfBuffers.size() << "buffers";
        return {};
    }

    Containers::Optional<Containers::Array<char>>& storage = _d->buffers[bufferId];
    if(storage) return Containers::ArrayView<const char>{*storage};

    const Utility::JsonToken& gltfBuffer = _d->gltfBuffers[bufferId];

    Containers::ArrayView<const char> view;
    if(const Utility::JsonToken* gltfBufferUri = gltfBuffer.find("uri"_s)) {
        if(!_d->gltf->parseString(*gltfBufferUri)) {
            Error{} << errorPrefix << "buffer" << bufferId << "has invalid uri property";
            return {};
        }
        if(!(storage = loadUri(errorPrefix, gltfBufferUri->asString())))
            return {};
        view = *storage;
    } else {
        /* URI may only be empty for buffers referencing the glb binary blob */
        if(bufferId != 0 || !_d->binChunk) {
            Error{} << errorPrefix << "buffer" << bufferId << "has missing uri property";
            return {};
        }
        view = *_d->binChunk;
    }

    /* Each buffer object is accessed only once so it doesn't make sense to
       cache the parsed size */
    const Utility::JsonToken* const gltfBufferByteLength = gltfBuffer.find("byteLength"_s);
    if(!gltfBufferByteLength || !_d->gltf->parseSize(*gltfBufferByteLength)) {
        Error{} << errorPrefix << "buffer" << bufferId
            << "has missing or invalid byteLength property";
        return {};
    }

    /* The spec mentions that non-GLB buffer length can be greater than
       byteLength. GLB buffer chunks may also be up to 3 bytes larger than
       byteLength because of padding. So we can't check for equality. */
    if(view.size() < gltfBufferByteLength->asSize()) {
        Error{} << errorPrefix << "buffer" << bufferId << "is too short, expected"
            << gltfBufferByteLength->asSize() << "bytes but got" << view.size();
        return {};
    }

    return view;
}

Containers::Optional<Containers::Triple<Containers::ArrayView<const char>, UnsignedInt, UnsignedInt>> GltfImporter::parseBufferView(const char* const errorPrefix, const UnsignedInt bufferViewId) {
    if(bufferViewId >= _d->gltfBufferViews.size()) {
        Error{} << errorPrefix << "buffer view index" << bufferViewId << "out of range for" << _d->gltfBufferViews.size() << "buffer views";
        return {};
    }

    /* Return if the buffer view is already parsed */
    Containers::Optional<Containers::Triple<Containers::ArrayView<const char>, UnsignedInt, UnsignedInt>>& storage = _d->bufferViews[bufferViewId];
    if(storage) return storage;

    const Utility::JsonToken& gltfBufferView = _d->gltfBufferViews[bufferViewId];
    const Utility::JsonToken* const gltfBufferId = gltfBufferView.find("buffer"_s);
    if(!gltfBufferId || !(_d->gltf->parseUnsignedInt(*gltfBufferId))) {
        Error{} << errorPrefix << "buffer view" << bufferViewId
            << "has missing or invalid buffer property";
        return {};
    }

    /* Get the buffer early and continue only if that doesn't fail. This also
       checks that the buffer ID is in bounds. */
    Containers::Optional<Containers::ArrayView<const char>> buffer = parseBuffer(errorPrefix, gltfBufferId->asUnsignedInt());
    if(!buffer) return {};

    /* Byte offset is optional, defaulting to 0 */
    const Utility::JsonToken* const gltfByteOffset = gltfBufferView.find("byteOffset"_s);
    if(gltfByteOffset && !_d->gltf->parseSize(*gltfByteOffset)) {
        Error{} << errorPrefix << "buffer view" << bufferViewId << "has invalid byteOffset property";
        return {};
    }

    const Utility::JsonToken* const gltfByteLength = gltfBufferView.find("byteLength"_s);
    if(!gltfByteLength || !_d->gltf->parseSize(*gltfByteLength)) {
        Error{} << errorPrefix << "buffer view" << bufferViewId << "has missing or invalid byteLength property";
        return {};
    }

    /* Byte stride is optional, if not set it's tightly packed. Assuming it's
       not larger than 4 GB -- glTF itself has the limit much lower (252, heh),
       but we don't really need to go that low. */
    const Utility::JsonToken* const gltfByteStride = gltfBufferView.find("byteStride"_s);
    if(gltfByteStride && !_d->gltf->parseUnsignedInt(*gltfByteStride)) {
        Error{} << errorPrefix << "buffer view" << bufferViewId << "has invalid byteStride property";
        return {};
    }

    const std::size_t offset = gltfByteOffset ? gltfByteOffset->asSize() : 0;
    const std::size_t requiredBufferSize = offset + gltfByteLength->asSize();
    if(buffer->size() < requiredBufferSize) {
        Error{} << errorPrefix << "buffer view" << bufferViewId << "needs" << requiredBufferSize << "bytes but buffer" << gltfBufferId->asUnsignedInt() << "has only" << buffer->size();
        return {};
    }

    /* If the buffer isn't strided, the first dimension has a zero stride and
       the second is the whole view */
    storage.emplace(
        buffer->slice(offset, offset + gltfByteLength->asSize()),
        gltfByteStride ? gltfByteStride->asUnsignedInt() : 0,
        gltfBufferId->asUnsignedInt());

    return storage;
}

Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>> GltfImporter::parseAccessor(const char* const errorPrefix, const UnsignedInt accessorId) {
    if(accessorId >= _d->gltfAccessors.size()) {
        Error{} << errorPrefix << "accessor index" << accessorId << "out of range for" << _d->gltfAccessors.size() << "accessors";
        return {};
    }

    /* Return if the buffer view is already parsed */
    Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>>& storage = _d->accessors[accessorId];
    if(storage) return storage;

    const Utility::JsonToken& gltfAccessor = _d->gltfAccessors[accessorId];

    /** @todo Validate alignment rules, calculate correct stride in accessorView():
        https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#data-alignment */

    if(gltfAccessor.find("sparse"_s)) {
        Error{} << errorPrefix << "accessor" << accessorId << "is using sparse storage, which is unsupported";
        return {};
    }

    /* Buffer views are optional in accessors, we're supposed to fill the view
       with zeros. Only makes sense with sparse data and we don't support
       that, so we require the bufferViewId to be present. */
    const Utility::JsonToken* const gltfBufferViewId = gltfAccessor.find("bufferView"_s);
    if(!gltfBufferViewId || !_d->gltf->parseUnsignedInt(*gltfBufferViewId)) {
        Error{} << errorPrefix << "accessor" << accessorId << "has missing or invalid bufferView property";
        return {};
    }

    /* Get the buffer view early and continue only if that doesn't fail. This
       also checks that the buffer view ID is in bounds. */
    Containers::Optional<Containers::Triple<Containers::ArrayView<const char>, UnsignedInt, UnsignedInt>> bufferView = parseBufferView(errorPrefix, gltfBufferViewId->asUnsignedInt());
    if(!bufferView) return {};

    /* Byte offset is optional, defaulting to 0 */
    const Utility::JsonToken* const gltfAccessorByteOffset = gltfAccessor.find("byteOffset"_s);
    if(gltfAccessorByteOffset && !_d->gltf->parseSize(*gltfAccessorByteOffset)) {
        Error{} << errorPrefix << "accessor" << accessorId << "has invalid byteOffset property";
        return {};
    }

    const Utility::JsonToken* const gltfAccessorComponentType = gltfAccessor.find("componentType"_s);
    if(!gltfAccessorComponentType || !_d->gltf->parseUnsignedInt(*gltfAccessorComponentType)) {
        Error{} << errorPrefix << "accessor" << accessorId << "has missing or invalid componentType property";
        return {};
    }
    VertexFormat componentFormat;
    switch(gltfAccessorComponentType->asUnsignedInt()) {
        case Implementation::GltfTypeByte:
            componentFormat = VertexFormat::Byte;
            break;
        case Implementation::GltfTypeUnsignedByte:
            componentFormat = VertexFormat::UnsignedByte;
            break;
        case Implementation::GltfTypeShort:
            componentFormat = VertexFormat::Short;
            break;
        case Implementation::GltfTypeUnsignedShort:
            componentFormat = VertexFormat::UnsignedShort;
            break;
        /* Signed int not supported in glTF at the moment */
        case Implementation::GltfTypeUnsignedInt:
            componentFormat = VertexFormat::UnsignedInt;
            break;
        case Implementation::GltfTypeFloat:
            componentFormat = VertexFormat::Float;
            break;
        default:
            Error{} << errorPrefix << "accessor" << accessorId << "has invalid componentType" << gltfAccessorComponentType->asUnsignedInt();
            return {};
    }

    const Utility::JsonToken* const gltfAccessorCount = gltfAccessor.find("count"_s);
    if(!gltfAccessorCount || !_d->gltf->parseSize(*gltfAccessorCount)) {
        Error{} << errorPrefix << "accessor" << accessorId << "has missing or invalid count property";
        return {};
    }

    const Utility::JsonToken* const gltfAccessorType = gltfAccessor.find("type"_s);
    if(!gltfAccessorType || !_d->gltf->parseString(*gltfAccessorType)) {
        Error{} << errorPrefix << "accessor" << accessorId << "has missing or invalid type property";
        return {};
    }
    const Containers::StringView accessorType = gltfAccessorType->asString();
    UnsignedInt componentCount, vectorCount;
    if(accessorType == "SCALAR"_s) {
        componentCount = 1;
        vectorCount = 1;
    } else if(accessorType == "VEC2"_s) {
        componentCount = 2;
        vectorCount = 1;
    } else if(accessorType == "VEC3"_s) {
        componentCount = 3;
        vectorCount = 1;
    } else if(accessorType == "VEC4"_s) {
        componentCount = 4;
        vectorCount = 1;
    } else if(accessorType == "MAT2"_s) {
        componentCount = vectorCount = 2;
    } else if(accessorType == "MAT3"_s) {
        componentCount = vectorCount = 3;
    } else if(accessorType == "MAT4"_s) {
        componentCount = vectorCount = 4;
    } else {
        Error{} << errorPrefix << "accessor" << accessorId << "has invalid type" << accessorType;
        return {};
    }

    /* Normalized is optional, defaulting to false */
    const Utility::JsonToken* const gltfAccessorNormalized = gltfAccessor.find("normalized"_s);
    if(gltfAccessorNormalized && !_d->gltf->parseBool(*gltfAccessorNormalized)) {
        Error{} << errorPrefix << "accessor" << accessorId << "has invalid normalized property";
        return {};
    }

    /* Check for illegal normalized types */
    if(gltfAccessorNormalized && gltfAccessorNormalized->asBool()) {
        if(componentFormat == VertexFormat::UnsignedInt ||
           componentFormat == VertexFormat::Float) {
            /* Since we're abusing VertexFormat for all formats, print just the
               enum value without the prefix to avoid cofusion */
            Error{} << errorPrefix << "accessor" << accessorId << "with component format" << Debug::packed << componentFormat << "can't be normalized";
            return {};
        }
    }

    /* We have only few allowed matrix types */
    if(vectorCount != 1 && componentFormat != VertexFormat::Float &&
        !(componentFormat == VertexFormat::Byte && gltfAccessorNormalized && gltfAccessorNormalized->asBool()) &&
        !(componentFormat == VertexFormat::Short && gltfAccessorNormalized && gltfAccessorNormalized->asBool())
    ) {
        /* Compose the normalized bit into the component format for printing.
           This shouldn't assert as we checked for illegal normalized types
           right above. Also, since we're abusing VertexFormat for all formats,
           print just the enum value without the prefix to avoid cofusion. */
        Error{} << errorPrefix << "accessor" << accessorId << "has an unsupported matrix component format" << Debug::packed << vertexFormat(componentFormat, 1, gltfAccessorNormalized && gltfAccessorNormalized->asBool());
        return {};
    }

    VertexFormat format;
    if(vectorCount == 1)
        format = vertexFormat(componentFormat, componentCount, gltfAccessorNormalized && gltfAccessorNormalized->asBool());
    else
        format = vertexFormat(componentFormat, vectorCount, componentCount, true);

    const std::size_t typeSize = vertexFormatSize(format);
    if(bufferView->second() && bufferView->second() < typeSize) {
        Error{} << errorPrefix << typeSize << Debug::nospace << "-byte type defined by accessor" << accessorId << "can't fit into buffer view" << gltfBufferViewId->asUnsignedInt() << "stride of" << bufferView->second();
        return {};
    }

    const std::size_t offset = gltfAccessorByteOffset ? gltfAccessorByteOffset->asSize() : 0;
    const std::size_t stride = bufferView->second() ? bufferView->second() : typeSize;
    const std::size_t requiredBufferViewSize = offset + stride*(gltfAccessorCount->asSize() - 1) + typeSize;
    if(bufferView->first().size() < requiredBufferViewSize) {
        Error{} << errorPrefix << "accessor" << accessorId << "needs" << requiredBufferViewSize << "bytes but buffer view" << gltfBufferViewId->asUnsignedInt() << "has only" << bufferView->first().size();
        return {};
    }

    storage.emplace(
        /* glTF only requires buffer views to be large enough to fit the actual
           data, not to have the size large enough to fit `count*stride`
           elements. The StridedArrayView expects the latter, so we fake the
           vertexData size to satisfy the assert. For simplicity we overextend
           by the whole stride instead of `offset + typeSize`, relying on
           on the above bound checks. A similar workaround is in doMesh() when
           populating mesh attribute data. */
        /** @todo instead of faking the size, split the offset into offset in
            whole strides and the remainder (Math::div), then form the view
            with offset in whole strides and then "shift" the view by the
            remainder (once there's StridedArrayView::shift() or some such) */
        Containers::StridedArrayView2D<const char>{{bufferView->first(), bufferView->first().size() + stride},
            static_cast<const char*>(bufferView->first().data()) + offset,
            {gltfAccessorCount->asSize(), typeSize},
            {std::ptrdiff_t(stride), 1}},
        format,
        gltfBufferViewId->asUnsignedInt());

    return storage;
}

namespace {

void fillDefaultConfiguration(Utility::ConfigurationGroup& conf) {
    /** @todo horrible workaround, fix this properly */
    conf.setValue("ignoreRequiredExtensions", false);
    conf.setValue("optimizeQuaternionShortestPath", true);
    conf.setValue("normalizeQuaternions", true);
    conf.setValue("mergeAnimationClips", false);
    conf.setValue("phongMaterialFallback", true);
    conf.setValue("objectIdAttribute", "_OBJECT_ID");
}

}

GltfImporter::GltfImporter() {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

GltfImporter::GltfImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

GltfImporter::GltfImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter{manager} {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

GltfImporter::~GltfImporter() = default;

ImporterFeatures GltfImporter::doFeatures() const { return ImporterFeature::OpenData|ImporterFeature::FileCallback; }

bool GltfImporter::doIsOpened() const { return !!_d && _d->gltf; }

void GltfImporter::doClose() { _d = nullptr; }

void GltfImporter::doOpenFile(const Containers::StringView filename) {
    _d.reset(new Document);
    _d->filename.emplace(Containers::String::nullTerminatedGlobalView(filename));
    AbstractImporter::doOpenFile(filename);
}

void GltfImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    if(!_d) _d.reset(new Document);

    /* Copy file content. Take over the existing array or copy the data if we
       can't. We need to keep the data around as JSON tokens are views onto it
       and also for the GLB binary chunk. */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        _d->fileData = std::move(data);
    } else {
        _d->fileData = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, _d->fileData);
    }

    /* Since we just made a owning copy of the file data above, mark the JSON
       string view as global to avoid Utility::Json making its own owned copy
       again */
    Containers::StringView json{_d->fileData, _d->fileData.size(), Containers::StringViewFlag::Global};
    std::size_t jsonByteOffset = 0;

    /* If the file looks like a GLB, extract the JSON and BIN chunk out of it */
    if(json.hasPrefix("glTF"_s)) {
        if(_d->fileData.size() < sizeof(Implementation::GltfGlbHeader)) {
            Error{} << "Trade::GltfImporter::openData(): binary glTF too small, expected at least" << sizeof(Implementation::GltfGlbHeader) << "bytes but got only" << _d->fileData.size();
            return;
        }
        const auto& header = *reinterpret_cast<const Implementation::GltfGlbHeader*>(_d->fileData.data());
        if(header.version != 2) {
            Error{} << "Trade::GltfImporter::openData(): unsupported binary glTF version" << header.version;
            return;
        }
        if(_d->fileData.size() != header.length) {
            Error{} << "Trade::GltfImporter::openData(): binary glTF size mismatch, expected" << header.length << "bytes but got" << _d->fileData.size();
            return;
        }
        if(Containers::StringView{header.json.magic, 4} != "JSON"_s) {
            /** @todo use Debug::str (escaping non-printable characters)
                instead of the hex once it exists */
            Error{} << "Trade::GltfImporter::openData(): expected a JSON chunk, got" << reinterpret_cast<void*>(header.json.id);
            return;
        }

        const char* const jsonDataBegin = _d->fileData + sizeof(Implementation::GltfGlbHeader);
        const char* const jsonDataEnd = jsonDataBegin + header.json.length;
        if(jsonDataEnd > _d->fileData.end()) {
            Error{} << "Trade::GltfImporter::openData(): binary glTF size mismatch, expected" << header.json.length << "bytes for a JSON chunk but got only" << _d->fileData.end() - jsonDataBegin;
            return;
        }

        /* Update the JSON view to contain just the JSON data. Slicing so the
           global flag set above gets preserved. */
        json = json.slice(jsonDataBegin, jsonDataBegin + header.json.length);
        jsonByteOffset = jsonDataBegin - _d->fileData;

        /* Other chunks. The spec defines just the BIN chunk, but there can be
           additional chunks defined by extensions that we're expected to
           skip */
        const char* chunk = jsonDataEnd;
        while(chunk != _d->fileData.end()) {
            if(chunk + sizeof(Implementation::GltfGlbChunkHeader) > _d->fileData.end()) {
                Error{} << "Trade::GltfImporter::openData(): binary glTF chunk starting at" << chunk - _d->fileData.begin() << "too small, expected at least" << sizeof(Implementation::GltfGlbChunkHeader) << "bytes but got only" << _d->fileData.end() - chunk;
                return;
            }

            const auto& chunkHeader = *reinterpret_cast<const Implementation::GltfGlbChunkHeader*>(chunk);
            const char* const chunkDataBegin = chunk + sizeof(Implementation::GltfGlbChunkHeader);
            const char* const chunkDataEnd = chunkDataBegin + chunkHeader.length;
            if(chunkDataEnd > _d->fileData.end()) {
                Error{} << "Trade::GltfImporter::openData(): binary glTF size mismatch, expected" << chunkHeader.length << "bytes for a chunk starting at" << chunk - _d->fileData.begin() << "but got only" << _d->fileData.end() - chunkDataBegin;
                return;
            }

            /* If a BIN chunk, save it. There can be at most one, so a warning
               will be printed for the next ones */
            if(!_d->binChunk && Containers::StringView{chunkHeader.magic, 4} == "BIN\0"_s)
                _d->binChunk = Containers::arrayView(chunkDataBegin, chunkHeader.length);
            else
                /** @todo use Debug::str (escaping non-printable characters)
                    instead of the hex once it exists */
                Warning{} << "Trade::GltfImporter::openData(): ignoring chunk" << reinterpret_cast<void*>(chunkHeader.id) << "at" << chunk - _d->fileData.begin();
            chunk = chunkDataEnd;
        }
    }

    /** @todo this means that if openFile() got passed a global string, Json
        will still make a copy of it -- need a way to preserve the globalness
        inside non-owned String */
    Containers::Optional<Utility::Json> gltf = Utility::Json::fromString(json, _d->filename ? Containers::StringView{*_d->filename} : Containers::StringView{}, 0, jsonByteOffset);
    if(!gltf || !gltf->parseObject(gltf->root())) {
        Error{} << "Trade::GltfImporter::openData(): invalid JSON";
        return;
    }

    /* Check version */
    const Utility::JsonToken* const gltfAsset = gltf->root().find("asset"_s);
    if(!gltfAsset || !gltf->parseObject(*gltfAsset)) {
        Error{} << "Trade::GltfImporter::openData(): missing or invalid asset property";
        return;
    }
    const Utility::JsonToken* const gltfAssetVersion = gltfAsset->find("version"_s);
    if(!gltfAssetVersion || !gltf->parseString(*gltfAssetVersion)) {
        Error{} << "Trade::GltfImporter::openData(): missing or invalid asset version property";
        return;
    }
    /* Min version is optional */
    const Utility::JsonToken* const gltfAssetMinVersion = gltfAsset->find("minVersion"_s);
    if(gltfAssetMinVersion && !gltf->parseString(*gltfAssetMinVersion)) {
        Error{} << "Trade::GltfImporter::openData(): invalid asset minVersion property";
        return;
    }

    /* Major versions are forward- and backward-compatible, but minVersion can
       be used to require support for features added in new minor versions.
       So far there's only 2.0 so we can use an exact comparison. */
    if(gltfAssetMinVersion && gltfAssetMinVersion->asString() != "2.0"_s) {
        Error{} << "Trade::GltfImporter::openData(): unsupported minVersion" << gltfAssetMinVersion->asString() << Debug::nospace << ", expected 2.0";
        return;
    }
    if(!gltfAssetVersion->asString().hasPrefix("2."_s)) {
        Error{} << "Trade::GltfImporter::openData(): unsupported version" << gltfAssetVersion->asString() << Debug::nospace << ", expected 2.x";
        return;
    }

    /* Check required extensions. Every extension in extensionsRequired is
       required to "load and/or render an asset". */
    if(const Utility::JsonToken* gltfExtensionsRequired = gltf->root().find("extensionsRequired"_s)) {
        if(!gltf->parseArray(*gltfExtensionsRequired)) {
            Error{} << "Trade::GltfImporter::openData(): invalid extensionsRequired property";
            return;
        }

        /** @todo Allow ignoring specific extensions through a config option,
            e.g. ignoreRequiredExtension=KHR_materials_volume */
        const bool ignoreRequiredExtensions = configuration().value<bool>("ignoreRequiredExtensions");

        constexpr Containers::StringView supportedExtensions[]{
            "KHR_lights_punctual"_s,
            "KHR_materials_clearcoat"_s,
            "KHR_materials_pbrSpecularGlossiness"_s,
            "KHR_materials_unlit"_s,
            "KHR_mesh_quantization"_s,
            "KHR_texture_basisu"_s,
            "KHR_texture_transform"_s,
            "GOOGLE_texture_basis"_s,
            "MSFT_texture_dds"_s
        };

        /* M*N loop should be okay here, extensionsRequired should usually have
           no or very few entries. Consider binary search if the list of
           supported extensions reaches a few dozen. */
        for(Utility::JsonArrayItem gltfExtension: gltfExtensionsRequired->asArray()) {
            if(!gltf->parseString(gltfExtension)) {
                Error{} << "Trade::GltfImporter::openData(): invalid required extension" << gltfExtension.index();
                return;
            }

            const Containers::StringView extension = gltfExtension.value().asString();
            bool found = false;
            for(const auto& supported: supportedExtensions) {
                if(supported == extension) {
                    found = true;
                    break;
                }
            }

            if(!found) {
                if(ignoreRequiredExtensions) {
                    Warning{} << "Trade::GltfImporter::openData(): required extension" << extension << "not supported";
                } else {
                    Error{} << "Trade::GltfImporter::openData(): required extension" << extension << "not supported";
                    return;
                }
            }
        }
    }

    /* Populate arrays of glTF objects */
    const auto populate = [](Utility::Json& gltf, Containers::Array<Containers::Reference<const Utility::JsonToken>>& out, Containers::StringView key, const char* item) {
        if(const Utility::JsonToken* const gltfObjects = gltf.root().find(key)) {
            if(!gltf.parseArray(*gltfObjects)) {
                Error{} << "Trade::GltfImporter::openData(): invalid" << key << "property";
                return false;
            }
            for(Utility::JsonArrayItem const gltfObject: gltfObjects->asArray()) {
                if(!gltf.parseObject(gltfObject)) {
                    Error{} << "Trade::GltfImporter::openData(): invalid" << item << gltfObject.index();
                    return false;
                }

                arrayAppend(out, gltfObject.value());
            }
        }

        return true;
    };
    const auto populateWithName = [](Utility::Json& gltf, const Utility::JsonToken& root, Containers::Array<Containers::Pair<Containers::Reference<const Utility::JsonToken>, Containers::StringView>>& out, Containers::StringView key, const char* item) {
        if(const Utility::JsonToken* const gltfObjects = root.find(key)) {
            if(!gltf.parseArray(*gltfObjects)) {
                Error{} << "Trade::GltfImporter::openData(): invalid" << key << "property";
                return false;
            }
            for(Utility::JsonArrayItem gltfObject: gltfObjects->asArray()) {
                if(!gltf.parseObject(gltfObject)) {
                    Error{} << "Trade::GltfImporter::openData(): invalid" << item << gltfObject.index();
                    return false;
                }

                const Utility::JsonToken* const gltfName = gltfObject.value().find("name"_s);
                if(gltfName && !gltf.parseString(*gltfName)) {
                    Error{} << "Trade::GltfImporter::openData(): invalid" << item << gltfObject.index() << "name property";
                    return false;
                }

                arrayAppend(out, InPlaceInit, gltfObject.value(), gltfName ? gltfName->asString() : Containers::StringView{});
            }
        }

        return true;
    };
    const auto populateExtensionWithName = [](Utility::Json& gltf, const Utility::JsonToken& extension, Containers::Array<Containers::Pair<Containers::Reference<const Utility::JsonToken>, Containers::StringView>>& out, Containers::StringView key, const char* item) {
        if(!gltf.parseObject(extension)) {
            Error{} << "Trade::GltfImporter::openData(): invalid" << extension.parent()->asString() << "extension";
            return false;
        }

        if(const Utility::JsonToken* const gltfObjects = extension.find(key)) {
            if(!gltf.parseArray(*gltfObjects)) {
                Error{} << "Trade::GltfImporter::openData(): invalid" << extension.parent()->asString() << key << "property";
                return false;
            }
            for(Utility::JsonArrayItem gltfObject: gltfObjects->asArray()) {
                if(!gltf.parseObject(gltfObject)) {
                    Error{} << "Trade::GltfImporter::openData(): invalid" << extension.parent()->asString() << item << gltfObject.index();
                    return false;
                }

                const Utility::JsonToken* const gltfName = gltfObject.value().find("name"_s);
                if(gltfName && !gltf.parseString(*gltfName)) {
                    Error{} << "Trade::GltfImporter::openData(): invalid" << extension.parent()->asString() << item << gltfObject.index() << "name property";
                    return false;
                }

                arrayAppend(out, InPlaceInit, gltfObject.value(), gltfName ? gltfName->asString() : Containers::StringView{});
            }
        }

        return true;
    };
    if(!populate(*gltf, _d->gltfBuffers, "buffers"_s, "buffer") ||
       !populate(*gltf, _d->gltfBufferViews, "bufferViews"_s, "buffer view") ||
       !populate(*gltf, _d->gltfAccessors, "accessors"_s, "accessor") ||
       !populate(*gltf, _d->gltfSamplers, "samplers"_s, "sampler") ||
       !populateWithName(*gltf, gltf->root(), _d->gltfNodes, "nodes"_s, "node") ||
       !populateWithName(*gltf, gltf->root(), _d->gltfMeshes, "meshes"_s, "mesh") ||
       /* Mesh primitives done below */
       !populateWithName(*gltf, gltf->root(), _d->gltfCameras, "cameras"_s, "camera") ||
       /* Light taken from an extension, done below */
       !populateWithName(*gltf, gltf->root(), _d->gltfAnimations, "animations"_s, "animation") ||
       !populateWithName(*gltf, gltf->root(), _d->gltfSkins, "skins"_s, "skin") ||
       !populateWithName(*gltf, gltf->root(), _d->gltfImages, "images"_s, "image") ||
       !populateWithName(*gltf, gltf->root(), _d->gltfTextures, "textures"_s, "texture") ||
       !populateWithName(*gltf, gltf->root(), _d->gltfMaterials, "materials"_s, "material") ||
       !populateWithName(*gltf, gltf->root(), _d->gltfScenes, "scenes"_s, "scene")
    ) return;

    /* Extensions */
    if(const Utility::JsonToken* const gltfExtensions = gltf->root().find("extensions"_s)) {
        if(!gltf->parseObject(*gltfExtensions)) {
            Error{} << "Trade::GltfImporter::openData(): invalid extensions property";
            return;
        }

        /* Lights */
        if(const Utility::JsonToken* const gltfKhrLightsPunctual = gltfExtensions->find("KHR_lights_punctual"_s)) {
            /* This doesn't check that the lights property is actually there
               (which is required by the spec), but that's fine -- if it'd ever
               get to core glTF, it would become optional */
            if(!populateExtensionWithName(*gltf, *gltfKhrLightsPunctual, _d->gltfLights, "lights"_s, "light"))
                return;
        }
    }

    /* Find cycles in node tree. The Tortoise and Hare algorithm relies on
       elements of the graph having a single outgoing edge, which means we have
       to build parent links first. During that process we check that nodes
       don't have multiple parents. */
    {
        /* Mark all nodes as unreferenced (-2) first -- if a node isn't
           referenced from any scene nodes or node children array, it'll stay
           that way */
        /** @todo this could be eventually used to compile a "leftovers" scene
            out of unreferenced nodes */
        Containers::Array<Int> nodeParents{DirectInit, _d->gltfNodes.size(), -2};

        /* Mark all nodes referenced by a scene as root nodes (-1) */
        for(std::size_t i = 0; i != _d->gltfScenes.size(); ++i) {
            const Utility::JsonToken* const gltfSceneNodes = _d->gltfScenes[i].first()->find("nodes"_s);
            if(!gltfSceneNodes) continue;

            const Containers::Optional<Containers::StridedArrayView1D<const UnsignedInt>> sceneNodes = gltf->parseUnsignedIntArray(*gltfSceneNodes);
            if(!sceneNodes) {
                Error{} << "Trade::GltfImporter::openData(): invalid nodes property of scene" << i;
                return;
            }

            for(const UnsignedInt node: *sceneNodes) {
                if(node >= _d->gltfNodes.size()) {
                    Error{} << "Trade::GltfImporter::openData(): node index" << node << "in scene" << i << "out of range for" << _d->gltfNodes.size() << "nodes";
                    return;
                }

                /* In this case it's fine if a node is referenced by multiple
                   scenes (and it's allowed by glTF) */
                nodeParents[node] = -1;
            }
        }

        /* Go through the node hierarchy and mark nested children, discovering
           potential conflicting parent nodes */
        for(std::size_t i = 0; i != _d->gltfNodes.size(); ++i) {
            const Utility::JsonToken* const gltfNodeChildren = _d->gltfNodes[i].first()->find("children"_s);
            if(!gltfNodeChildren) continue;

            const Containers::Optional<Containers::StridedArrayView1D<const UnsignedInt>> nodeChildren = gltf->parseUnsignedIntArray(*gltfNodeChildren);
            if(!nodeChildren) {
                Error{} << "Trade::GltfImporter::openData(): invalid children property of node" << i;
                return;
            }

            for(const UnsignedInt child: *nodeChildren) {
                if(child >= _d->gltfNodes.size()) {
                    Error{} << "Trade::GltfImporter::openData(): child index" << child << "in node" << i << "out of range for" << _d->gltfNodes.size() << "nodes";
                    return;
                }

                /* If a referenced child already has a parent assigned, it's a
                   cycle */
                if(nodeParents[child] == -1) {
                    Error{} << "Trade::GltfImporter::openData(): node" << child << "is both a root node and a child of node" << i;
                    return;
                } else if(nodeParents[child] != -2) {
                    Error{} << "Trade::GltfImporter::openData(): node" << child << "is a child of both node" << nodeParents[child] << "and node" << i;
                    return;
                }

                nodeParents[child] = i;
            }
        }

        /* Find cycles, Tortoise and Hare */
        for(std::size_t i = 0; i != _d->gltfNodes.size(); ++i) {
            Int p1 = nodeParents[i];
            Int p2 = p1 < 0 ? -1 : nodeParents[p1];

            while(p1 >= 0 && p2 >= 0) {
                if(p1 == p2) {
                    Error{} << "Trade::GltfImporter::openData(): node tree contains cycle starting at node" << i;
                    return;
                }

                p1 = nodeParents[p1];
                p2 = nodeParents[p2] < 0 ? -1 : nodeParents[nodeParents[p2]];
            }
        }
    }

    /* Treat meshes with multiple primitives as separate meshes. Each mesh gets
       duplicated as many times as is the size of the primitives array.
       Conservatively reserve for exactly one primitive per mesh, as that's the
       most common case. */
    arrayReserve(_d->gltfMeshPrimitiveMap, _d->gltfMeshes.size());
    _d->meshSizeOffsets = Containers::Array<std::size_t>{_d->gltfMeshes.size() + 1};
    _d->meshSizeOffsets[0] = 0;
    for(std::size_t i = 0; i != _d->gltfMeshes.size(); ++i) {
        const Utility::JsonToken* gltfMeshPrimitives = _d->gltfMeshes[i].first()->find("primitives"_s);
        if(!gltfMeshPrimitives || !gltf->parseArray(*gltfMeshPrimitives)) {
            Error{} << "Trade::GltfImporter::openData(): missing or invalid primitives property in mesh" << i;
            return;
        }

        /* Yes, this isn't array item count but rather a size of the whole
           subtree, but that's fine as we only check it's non-empty */
        if(gltfMeshPrimitives->childCount() == 0) {
            Error{} << "Trade::GltfImporter::openData(): mesh" << i << "has no primitives";
            return;
        }

        for(Utility::JsonArrayItem gltfPrimitive: gltfMeshPrimitives->asArray()) {
            if(!gltf->parseObject(gltfPrimitive.value())) {
                Error{} << "Trade::GltfImporter::openData(): invalid mesh" << i << "primitive" << gltfPrimitive.index();
                return;
            }

            arrayAppend(_d->gltfMeshPrimitiveMap, InPlaceInit, i, gltfPrimitive.value());
        }

        _d->meshSizeOffsets[i + 1] = _d->gltfMeshPrimitiveMap.size();
    }

    /* Go through all meshes, collect custom attributes and decide about
       implicitly enabling textureCoordinateYFlipInMaterial if it isn't already
       requested from the configuration and there are any texture coordinates
       that need it */
    if(configuration().value<bool>("textureCoordinateYFlipInMaterial"))
        _d->textureCoordinateYFlipInMaterial = true;
    for(std::size_t i = 0; i != _d->gltfMeshPrimitiveMap.size(); ++i) {
        const Utility::JsonToken& gltfPrimitive = _d->gltfMeshPrimitiveMap[i].second();

        /* The glTF spec requires a primitive to define an attribute property
           with at least one attribute, but we're fine without here. Stricter
           checks, if any, are done in doMesh(). */
        const Utility::JsonToken* gltfAttributes = gltfPrimitive.find("attributes"_s);
        if(!gltfAttributes) continue;

        if(!gltf->parseObject(*gltfAttributes)) {
            Error{} << "Trade::GltfImporter::openData(): invalid primitive attributes property in mesh" << _d->gltfMeshPrimitiveMap[i].first();
            return;
        }

        for(Utility::JsonObjectItem gltfAttribute: gltfAttributes->asObject()) {
            /* Decide about texture coordinate Y flipping if not set already */
            if(gltfAttribute.key().hasPrefix("TEXCOORD_"_s) && isBuiltinNumberedMeshAttribute(gltfAttribute.key())) {
                if(_d->textureCoordinateYFlipInMaterial) continue;

                /* Perform a subset of parsing and validation done in doMesh()
                   and parseAccessor(). Not calling parseAccessor() here because it
                   would cause the actual buffers to be loaded and a ton other
                   validation performed, which is undesirable during the
                   initial file opening.

                   On the other hand, for simplicity also not making doMesh()
                   or parseAccessor() assume any of this was already parsed,
                   except for validation of the attributes object in the outer
                   loop, which is guaranteed to be done for all meshes. */

                if(!gltf->parseUnsignedInt(gltfAttribute.value())) {
                    Error{} << "Trade::GltfImporter::openData(): invalid attribute" << gltfAttribute.key() << "in mesh" << _d->gltfMeshPrimitiveMap[i].first();
                    return;
                }
                if(gltfAttribute.value().asUnsignedInt() >= _d->gltfAccessors.size()) {
                    Error{} << "Trade::GltfImporter::openData(): accessor index" << gltfAttribute.value().asUnsignedInt() << "out of range for" << _d->gltfAccessors.size() << "accessors";
                    return;
                }

                const Utility::JsonToken& gltfAccessor = _d->gltfAccessors[gltfAttribute.value().asUnsignedInt()];

                const Utility::JsonToken* const gltfAccessorComponentType = gltfAccessor.find("componentType"_s);
                if(!gltfAccessorComponentType || !gltf->parseUnsignedInt(*gltfAccessorComponentType)) {
                    Error{} << "Trade::GltfImporter::openData(): accessor" << gltfAttribute.value().asUnsignedInt() << "has missing or invalid componentType property";
                    return;
                }

                /* Normalized is optional, defaulting to false */
                const Utility::JsonToken* const gltfAccessorNormalized = gltfAccessor.find("normalized"_s);
                if(gltfAccessorNormalized && !gltf->parseBool(*gltfAccessorNormalized)) {
                    Error{} << "Trade::GltfImporter::openData(): accessor" << gltfAttribute.value().asUnsignedInt() << "has invalid normalized property";
                    return;
                }

                const UnsignedInt accessorComponentType = gltfAccessorComponentType->asUnsignedInt();
                const bool normalized = gltfAccessorNormalized && gltfAccessorNormalized->asBool();
                if(accessorComponentType == Implementation::GltfTypeByte ||
                   accessorComponentType == Implementation::GltfTypeShort ||
                  (accessorComponentType == Implementation::GltfTypeUnsignedByte && !normalized) ||
                  (accessorComponentType == Implementation::GltfTypeUnsignedShort && !normalized))
                {
                    Debug{} << "Trade::GltfImporter::openData(): file contains non-normalized texture coordinates, implicitly enabling textureCoordinateYFlipInMaterial";
                    _d->textureCoordinateYFlipInMaterial = true;
                }

            /* If the name isn't recognized, add the attribute to custom if not
               there already */
            } else if(!isBuiltinMeshAttribute(configuration(), gltfAttribute.key())) {
                /* The spec says that all user-defined attributes must
                   start with an underscore. We don't really care and just
                   print a warning. */
                if(!gltfAttribute.key().hasPrefix("_"_s))
                    Warning{} << "Trade::GltfImporter::openData(): unknown attribute" << gltfAttribute.key() << Debug::nospace << ", importing as custom attribute";

                if(_d->meshAttributesForName.emplace(gltfAttribute.key(),
                    meshAttributeCustom(_d->meshAttributeNames.size())).second
                )
                    arrayAppend(_d->meshAttributeNames, gltfAttribute.key());
            }
        }
    }

    /* Parse default scene, as we can't fail in doDefaultScene() */
    if(const Utility::JsonToken* gltfScene = gltf->root().find("scene"_s)) {
        if(!gltf->parseUnsignedInt(*gltfScene)) {
            Error{} << "Trade::GltfImporter::openData(): invalid scene property";
            return;
        }
        if(gltfScene->asUnsignedInt() >= _d->gltfScenes.size()) {
            Error{} << "Trade::GltfImporter::openData(): scene index" << gltfScene->asUnsignedInt() << "out of range for" << _d->gltfScenes.size() << "scenes";
            return;
        }
    }

    /* All good, save the parsed state */
    _d->gltf = std::move(gltf);

    /* Allocate storage for parsed buffers, buffer views and accessors */
    _d->buffers = Containers::Array<Containers::Optional<Containers::Array<char>>>{_d->gltfBuffers.size()};
    _d->bufferViews = Containers::Array<Containers::Optional<Containers::Triple<Containers::ArrayView<const char>, UnsignedInt, UnsignedInt>>>{_d->gltfBufferViews.size()};
    _d->accessors = Containers::Array<Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>>>{_d->gltfAccessors.size()};
    _d->samplers = Containers::Array<Containers::Optional<Document::Sampler>>{_d->gltfSamplers.size()};

    /* Name maps are lazy-loaded because these might not be needed every time */
}

UnsignedInt GltfImporter::doAnimationCount() const {
    /* If the animations are merged, there's at most one */
    if(configuration().value<bool>("mergeAnimationClips"))
        return _d->gltfAnimations.isEmpty() ? 0 : 1;

    return _d->gltfAnimations.size();
}

Int GltfImporter::doAnimationForName(const Containers::StringView name) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return -1;

    if(!_d->animationsForName) {
        _d->animationsForName.emplace();
        _d->animationsForName->reserve(_d->gltfAnimations.size());
        for(std::size_t i = 0; i != _d->gltfAnimations.size(); ++i)
            if(const Containers::StringView n = _d->gltfAnimations[i].second())
                _d->animationsForName->emplace(n, i);
    }

    const auto found = _d->animationsForName->find(name);
    return found == _d->animationsForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doAnimationName(const UnsignedInt id) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return {};
    return _d->gltfAnimations[id].second();
}

namespace {

template<class V> void postprocessSplineTrack(const UnsignedInt timeTrackUsed, const Containers::ArrayView<const Float> keys, const Containers::ArrayView<Math::CubicHermite<V>> values) {
    /* Already processed, don't do that again */
    if(timeTrackUsed != ~UnsignedInt{}) return;

    CORRADE_INTERNAL_ASSERT(keys.size() == values.size());
    if(keys.size() < 2) return;

    /* Convert the `a` values to `n` and the `b` values to `m` as described in
       https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#appendix-c-spline-interpolation
       Unfortunately I was not able to find any concrete name for this, so it's
       not part of the CubicHermite implementation but is kept here locally. */
    for(std::size_t i = 0; i < keys.size() - 1; ++i) {
        const Float timeDifference = keys[i + 1] - keys[i];
        values[i].outTangent() *= timeDifference;
        values[i + 1].inTangent() *= timeDifference;
    }
}

}

Containers::Optional<AnimationData> GltfImporter::doAnimation(UnsignedInt id) {
    /* Import either a single animation or all of them together. At the moment,
       Blender doesn't really support cinematic animations (affecting multiple
       objects): https://blender.stackexchange.com/q/5689. And since
       https://github.com/KhronosGroup/glTF-Blender-Exporter/pull/166, these
       are exported as a set of object-specific clips, which may not be wanted,
       so we give the users an option to merge them all together. */
    const std::size_t animationBegin =
        configuration().value<bool>("mergeAnimationClips") ? 0 : id;
    const std::size_t animationEnd =
        configuration().value<bool>("mergeAnimationClips") ? _d->gltfAnimations.size() : id + 1;

    const Containers::StridedArrayView1D<Containers::Reference<const Utility::JsonToken>> gltfAnimations = stridedArrayView(_d->gltfAnimations.slice(animationBegin, animationEnd)).slice(&decltype(_d->gltfAnimations)::Type::first);

    /* Parsed data for samplers in each processed animation. Stored in a
       contiguous array, data for sampler `j` of animation `i` is at
       `animationSamplerData[animationSamplerDataOffsets[i] + j]`. */
    struct AnimationSamplerData {
        UnsignedInt input;
        UnsignedInt output;
        Animation::Interpolation interpolation;
    };
    Containers::Array<AnimationSamplerData> animationSamplerData;
    Containers::Array<UnsignedInt> animationSamplerDataOffsets{NoInit, gltfAnimations.size() + 1};
    /* First gather the input and output data ranges. Key is unique accessor
       ID so we don't duplicate shared data, value is offset in the output data
       and ID of the corresponding key track in case given track is a spline
       interpolation. The time track ID is initialized to ~UnsignedInt{} and
       will be used later to check that a spline track was not used with more
       than one time track, as it needs to be postprocessed for given time
       track. */
    struct SamplerData {
        std::size_t outputOffset;
        UnsignedInt timeTrack;
    };
    std::unordered_map<UnsignedInt, SamplerData> samplerData;
    std::size_t dataSize = 0;
    for(std::size_t i = 0; i != gltfAnimations.size(); ++i) {
        const Utility::JsonToken& gltfAnimation = gltfAnimations[i];
        const Utility::JsonToken* const gltfAnimationSamplers = gltfAnimation.find("samplers"_s);
        if(!gltfAnimationSamplers || !_d->gltf->parseArray(*gltfAnimationSamplers)) {
            Error{} << "Trade::GltfImporter::animation(): missing or invalid samplers property";
            return {};
        }

        /* Save offset at which samplers for this animation will be stored */
        animationSamplerDataOffsets[i] = animationSamplerData.size();

        for(const Utility::JsonArrayItem gltfAnimationSampler: gltfAnimationSamplers->asArray()) {
            if(!_d->gltf->parseObject(gltfAnimationSampler)) {
                Error{} << "Trade::GltfImporter::animation(): invalid sampler" << gltfAnimationSampler.index();
                return {};
            }

            const Utility::JsonToken* const gltfAnimationSamplerInput = gltfAnimationSampler.value().find("input"_s);
            if(!gltfAnimationSamplerInput || !_d->gltf->parseUnsignedInt(*gltfAnimationSamplerInput)) {
                Error{} << "Trade::GltfImporter::animation(): missing or invalid sampler" << gltfAnimationSampler.index() << "input property";
                return {};
            }

            const Utility::JsonToken* const gltfAnimationSamplerOutput = gltfAnimationSampler.value().find("output"_s);
            if(!gltfAnimationSamplerOutput || !_d->gltf->parseUnsignedInt(*gltfAnimationSamplerOutput)) {
                Error{} << "Trade::GltfImporter::animation(): missing or invalid sampler" << gltfAnimationSampler.index() << "output property";
                return {};
            }

            /* Interpolation is optional, LINEAR if not present */
            const Utility::JsonToken* const gltfAnimationSamplerInterpolation = gltfAnimationSampler.value().find("interpolation"_s);
            if(gltfAnimationSamplerInterpolation &&  !_d->gltf->parseString(*gltfAnimationSamplerInterpolation)) {
                Error{} << "Trade::GltfImporter::animation(): invalid sampler" << gltfAnimationSampler.index() << "interpolation property";
                return {};
            }
            const Containers::StringView interpolationString = gltfAnimationSamplerInterpolation ? gltfAnimationSamplerInterpolation->asString() : "LINEAR"_s;
            Animation::Interpolation interpolation;
            if(interpolationString == "LINEAR"_s)
                interpolation = Animation::Interpolation::Linear;
            else if(interpolationString == "STEP"_s)
                interpolation = Animation::Interpolation::Constant;
            else if(interpolationString == "CUBICSPLINE"_s)
                interpolation = Animation::Interpolation::Spline;
            else {
                Error{} << "Trade::GltfImporter::animation(): unrecognized sampler" << gltfAnimationSampler.index() << "interpolation" << interpolationString;
                return {};
            }

            /** @todo handle alignment once we do more than just four-byte types */

            /* If the input view is not yet present in the output data buffer,
               add it */
            if(samplerData.find(gltfAnimationSamplerInput->asUnsignedInt()) == samplerData.end()) {
                const Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>> accessor = parseAccessor("Trade::GltfImporter::animation():", gltfAnimationSamplerInput->asUnsignedInt());
                if(!accessor)
                    return {};

                samplerData.emplace(gltfAnimationSamplerInput->asUnsignedInt(), SamplerData{dataSize, ~UnsignedInt{}});
                dataSize += accessor->first().size()[0]*accessor->first().size()[1];
            }

            /* If the output view is not yet present in the output data buffer,
               add it */
            if(samplerData.find(gltfAnimationSamplerOutput->asUnsignedInt()) == samplerData.end()) {
                const Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>> accessor = parseAccessor("Trade::GltfImporter::animation():", gltfAnimationSamplerOutput->asUnsignedInt());
                if(!accessor)
                    return {};

                samplerData.emplace(gltfAnimationSamplerOutput->asUnsignedInt(), SamplerData{dataSize, ~UnsignedInt{}});
                dataSize += accessor->first().size()[0]*accessor->first().size()[1];
            }

            arrayAppend(animationSamplerData, InPlaceInit,
                gltfAnimationSamplerInput->asUnsignedInt(),
                gltfAnimationSamplerOutput->asUnsignedInt(),
                interpolation);
        }
    }

    /* Save final size of animation samplers so we can unconditionally use
       `animationSamplerDataOffsets[i + 1] -  animationSamplerDataOffsets[i]`
       to get sampler count for animation `i` */
    animationSamplerDataOffsets[gltfAnimations.size()] = animationSamplerData.size();

    /* Populate the data array */
    /**
     * @todo Once memory-mapped files are supported, this can all go away
     *      except when spline tracks are present -- in that case we need to
     *      postprocess them and can't just use the memory directly.
     */
    Containers::Array<char> data{dataSize};
    for(const std::pair<const UnsignedInt, SamplerData>& view: samplerData) {
        /* The accessor should be already parsed from above, so just retrieve
           its view instead of going through parseAccessor() again */
        const Containers::StridedArrayView2D<const char> src =
            _d->accessors[view.first]->first();
        const Containers::StridedArrayView2D<char> dst{
            data.exceptPrefix(view.second.outputOffset), src.size()};
        Utility::copy(src, dst);
    }

    /* Calculate total track count. If merging all animations together, this is
       the sum of all clip track counts. */
    std::size_t trackCount = 0;
    for(const Utility::JsonToken& gltfAnimation: gltfAnimations) {
        const Utility::JsonToken* const gltfAnimationChannels = gltfAnimation.find("channels"_s);
        if(!gltfAnimationChannels || !_d->gltf->parseArray(*gltfAnimationChannels)) {
            Error{} << "Trade::GltfImporter::animation(): missing or invalid channels property";
            return {};
        }

        for(const Utility::JsonArrayItem gltfAnimationChannel: gltfAnimationChannels->asArray()) {
            if(!_d->gltf->parseObject(gltfAnimationChannel)) {
                Error{} << "Trade::GltfImporter::animation(): invalid channel" << gltfAnimationChannel.index();
                return {};
            }

            const Utility::JsonToken* const gltfAnimationChannelTarget = gltfAnimationChannel.value().find("target"_s);
            if(!gltfAnimationChannelTarget || !_d->gltf->parseObject(*gltfAnimationChannelTarget)) {
                Error{} << "Trade::GltfImporter::animation(): missing or invalid channel" << gltfAnimationChannel.index() << "target property";
                return {};
            }

            /* Skip animations without a target node. See comment below. Also,
               we're not using the node value for anything here, so further
               validation is done below. */
            if(gltfAnimationChannelTarget->find("node"_s))
                ++trackCount;
        }
    }

    /* Import all tracks */
    bool hadToRenormalize = false;
    std::size_t trackId = 0;
    Containers::Array<Trade::AnimationTrackData> tracks{trackCount};
    for(std::size_t i = 0; i != gltfAnimations.size(); ++i) {
        const Utility::JsonToken& gltfAnimation = gltfAnimations[i];
        /* Channels parsed and checked above already, so can go directly here */
        for(const Utility::JsonArrayItem gltfAnimationChannel: gltfAnimation["channels"_s].asArray()) {
            const Utility::JsonToken* const gltfSampler = gltfAnimationChannel.value().find("sampler"_s);
            if(!gltfSampler || !_d->gltf->parseUnsignedInt(*gltfSampler)) {
                Error{} << "Trade::GltfImporter::animation(): missing or invalid channel" << gltfAnimationChannel.index() << "sampler property";
                return {};
            }
            const std::size_t animationSamplerDataOffset = animationSamplerDataOffsets[i];
            if(gltfSampler->asUnsignedInt() >= animationSamplerDataOffsets[i + 1] - animationSamplerDataOffset) {
                Error{} << "Trade::GltfImporter::animation(): sampler index" << gltfSampler->asUnsignedInt() << "in channel" << gltfAnimationChannel.index() << "out of range for" << animationSamplerDataOffsets[i + 1] - animationSamplerDataOffset << "samplers";
                return {};
            }
            const AnimationSamplerData& sampler = animationSamplerData[animationSamplerDataOffset + gltfSampler->asUnsignedInt()];

            /* Skip animations without a target node. Consistent with
               tinygltf's behavior, currently there are no extensions for
               animating materials or anything else so there's no point in
               importing such animations. */
            const Utility::JsonToken& gltfTarget = gltfAnimationChannel.value()["target"_s];
            const Utility::JsonToken* gltfTargetNode = gltfTarget.find("node"_s);
            /** @todo revisit once KHR_animation2 is a thing:
                https://github.com/KhronosGroup/glTF/pull/2033 */
            if(!gltfTargetNode)
                continue;

            if(!_d->gltf->parseUnsignedInt(*gltfTargetNode)) {
                Error{} << "Trade::GltfImporter::animation(): invalid channel" << gltfAnimationChannel.index() << "target node property";
                return {};
            }
            if(gltfTargetNode->asUnsignedInt() >= _d->gltfNodes.size()) {
                Error{} << "Trade::GltfImporter::animation(): target node index" << gltfTargetNode->asUnsignedInt() << "in channel" << gltfAnimationChannel.index() << "out of range for" << _d->gltfNodes.size() << "nodes";
                return {};
            }

            /* Key properties -- always float time. Again, the accessor should
               be already parsed from above, so just retrieve its view instead
               of going through parseAccessor() again. */
            const Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>& input = *_d->accessors[sampler.input];
            if(input.second() != VertexFormat::Float) {
                /* Since we're abusing VertexFormat for all formats, print just
                   the enum value without the prefix to avoid cofusion */
                Error{} << "Trade::GltfImporter::animation(): channel" << gltfAnimationChannel.index() << "time track has unexpected type" << Debug::packed << input.second();
                return {};
            }

            /* View on the key data */
            const auto inputDataFound = samplerData.find(sampler.input);
            CORRADE_INTERNAL_ASSERT(inputDataFound != samplerData.end());
            const auto keys = Containers::arrayCast<Float>(
                data.exceptPrefix(inputDataFound->second.outputOffset).prefix(
                    input.first().size()[0]*
                    input.first().size()[1]));

            /* Decide on value properties. Again, the accessor should be
               already parsed from above, so just retrieve its view instead of
               going through parseAccessor() again. */
            const Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>& output = *_d->accessors[sampler.output];
            AnimationTrackTargetType target;
            AnimationTrackType type, resultType;
            Animation::TrackViewStorage<const Float> track;
            const auto outputDataFound = samplerData.find(sampler.output);
            CORRADE_INTERNAL_ASSERT(outputDataFound != samplerData.end());
            const auto outputData = data.exceptPrefix(outputDataFound->second.outputOffset)
                .prefix(output.first().size()[0]*
                        output.first().size()[1]);
            UnsignedInt& timeTrackUsed = outputDataFound->second.timeTrack;

            const std::size_t valuesPerKey = sampler.interpolation == Animation::Interpolation::Spline ? 3 : 1;
            if(input.first().size()[0]*valuesPerKey != output.first().size()[0]) {
                Error{} << "Trade::GltfImporter::animation(): channel" << gltfAnimationChannel.index() << "target track size doesn't match time track size, expected" << output.first().size()[0] << "but got" << input.first().size()[0]*valuesPerKey;
                return {};
            }

            const Utility::JsonToken* const gltfTargetPath = gltfTarget.find("path"_s);
            if(!gltfTargetPath || !_d->gltf->parseString(*gltfTargetPath)) {
                Error{} << "Trade::GltfImporter::animation(): missing or invalid channel" << gltfAnimationChannel.index() << "target path property";
                return {};
            }

            /* Translation */
            if(gltfTargetPath->asString() == "translation"_s) {
                if(output.second() != VertexFormat::Vector3) {
                    /* Since we're abusing VertexFormat for all formats, print
                       just the enum value without the prefix to avoid
                       cofusion */
                    Error{} << "Trade::GltfImporter::animation(): translation track has unexpected type" << Debug::packed << output.second();
                    return {};
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Translation3D;
                resultType = AnimationTrackType::Vector3;
                if(sampler.interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once
                       for every track -- postprocessSplineTrack() checks
                       that. */
                    const auto values = Containers::arrayCast<CubicHermite3D>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermite3D;
                    track = Animation::TrackView<const Float, const CubicHermite3D>{
                        keys, values, sampler.interpolation,
                        animationInterpolatorFor<CubicHermite3D>(sampler.interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    type = AnimationTrackType::Vector3;
                    track = Animation::TrackView<const Float, const Vector3>{keys,
                        Containers::arrayCast<Vector3>(outputData),
                        sampler.interpolation,
                        animationInterpolatorFor<Vector3>(sampler.interpolation),
                        Animation::Extrapolation::Constant};
                }

            /* Rotation */
            } else if(gltfTargetPath->asString() == "rotation"_s) {
                /** @todo rotation can be also normalized (?!) to a vector of 8/16bit (signed?!) integers */

                if(output.second() != VertexFormat::Vector4) {
                    /* Since we're abusing VertexFormat for all formats, print
                       just the enum value without the prefix to avoid
                       cofusion */
                    Error{} << "Trade::GltfImporter::animation(): rotation track has unexpected type" << Debug::packed << output.second();
                    return {};
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Rotation3D;
                resultType = AnimationTrackType::Quaternion;
                if(sampler.interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once
                       for every track -- postprocessSplineTrack() checks
                       that. */
                    const auto values = Containers::arrayCast<CubicHermiteQuaternion>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermiteQuaternion;
                    track = Animation::TrackView<const Float, const CubicHermiteQuaternion>{
                        keys, values, sampler.interpolation,
                        animationInterpolatorFor<CubicHermiteQuaternion>(sampler.interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    /* Ensure shortest path is always chosen. Not doing this
                       for spline interpolation, there it would cause war and
                       famine. */
                    const auto values = Containers::arrayCast<Quaternion>(outputData);
                    if(configuration().value<bool>("optimizeQuaternionShortestPath")) {
                        Float flip = 1.0f;
                        for(std::size_t j = 0; j + 1 < values.size(); ++j) {
                            if(Math::dot(values[j], values[j + 1]*flip) < 0) flip = -flip;
                            values[j + 1] *= flip;
                        }
                    }

                    /* Normalize the quaternions if not already. Don't attempt
                       to normalize every time to avoid tiny differences, only
                       when the quaternion looks to be off. Again, not doing
                       this for splines as it would cause things to go
                       haywire. */
                    if(configuration().value<bool>("normalizeQuaternions")) {
                        for(auto& quat: values) if(!quat.isNormalized()) {
                            quat = quat.normalized();
                            hadToRenormalize = true;
                        }
                    }

                    type = AnimationTrackType::Quaternion;
                    track = Animation::TrackView<const Float, const Quaternion>{
                        keys, values, sampler.interpolation,
                        animationInterpolatorFor<Quaternion>(sampler.interpolation),
                        Animation::Extrapolation::Constant};
                }

            /* Scale */
            } else if(gltfTargetPath->asString() == "scale"_s) {
                if(output.second() != VertexFormat::Vector3) {
                    /* Since we're abusing VertexFormat for all formats, print
                       just the enum value without the prefix to avoid
                       cofusion */
                    Error{} << "Trade::GltfImporter::animation(): scaling track has unexpected type" << Debug::packed << output.second();
                    return {};
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Scaling3D;
                resultType = AnimationTrackType::Vector3;
                if(sampler.interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once
                       for every track -- postprocessSplineTrack() checks
                       that. */
                    const auto values = Containers::arrayCast<CubicHermite3D>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermite3D;
                    track = Animation::TrackView<const Float, const CubicHermite3D>{
                        keys, values, sampler.interpolation,
                        animationInterpolatorFor<CubicHermite3D>(sampler.interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    type = AnimationTrackType::Vector3;
                    track = Animation::TrackView<const Float, const Vector3>{keys,
                        Containers::arrayCast<Vector3>(outputData),
                        sampler.interpolation,
                        animationInterpolatorFor<Vector3>(sampler.interpolation),
                        Animation::Extrapolation::Constant};
                }

            } else {
                Error{} << "Trade::GltfImporter::animation(): unsupported track target" << gltfTargetPath->asString();
                return {};
            }

            /* Splines were postprocessed using the corresponding time track.
               If a spline is not yet marked as postprocessed, mark it.
               Otherwise check that the spline track is always used with the
               same time track. */
            if(sampler.interpolation == Animation::Interpolation::Spline) {
                if(timeTrackUsed == ~UnsignedInt{})
                    timeTrackUsed = sampler.input;
                else if(timeTrackUsed != sampler.input) {
                    Error{} << "Trade::GltfImporter::animation(): spline track is shared with different time tracks, we don't support that, sorry";
                    return {};
                }
            }

            tracks[trackId++] = AnimationTrackData{type, resultType, target,
                gltfTargetNode->asUnsignedInt(), track};
        }
    }

    if(hadToRenormalize)
        Warning{} << "Trade::GltfImporter::animation(): quaternions in some rotation tracks were renormalized";

    return AnimationData{std::move(data), std::move(tracks),
        configuration().value<bool>("mergeAnimationClips") ? nullptr :
        &*_d->gltfAnimations[id].first()};
}

UnsignedInt GltfImporter::doCameraCount() const {
    return _d->gltfCameras.size();
}

Int GltfImporter::doCameraForName(const Containers::StringView name) {
    if(!_d->camerasForName) {
        _d->camerasForName.emplace();
        _d->camerasForName->reserve(_d->gltfCameras.size());
        for(std::size_t i = 0; i != _d->gltfCameras.size(); ++i)
            if(const Containers::StringView n = _d->gltfCameras[i].second())
                _d->camerasForName->emplace(n , i);
    }

    const auto found = _d->camerasForName->find(name);
    return found == _d->camerasForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doCameraName(const UnsignedInt id) {
    return _d->gltfCameras[id].second();
}

Containers::Optional<CameraData> GltfImporter::doCamera(const UnsignedInt id) {
    const Utility::JsonToken& gltfCamera = _d->gltfCameras[id].first();

    const Utility::JsonToken* const gltfType = gltfCamera.find("type"_s);
    if(!gltfType || !_d->gltf->parseString(*gltfType)) {
        Error{} << "Trade::GltfImporter::camera(): missing or invalid type property";
        return {};
    }

    /* https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#projection-matrices */

    /* Perspective camera */
    if(gltfType->asString() == "perspective"_s) {
        const Utility::JsonToken* const gltfPerspectiveCamera = gltfCamera.find("perspective"_s);
        if(!gltfPerspectiveCamera || !_d->gltf->parseObject(*gltfPerspectiveCamera)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid perspective property";
            return {};
        }

        /* Aspect ratio is optional, use 1:1 if not set */
        /** @todo spec says "if not set "aspect ratio of the rendering viewport
            MUST be used", heh, how am I supposed to know that here? */
        const Utility::JsonToken* const gltfAspectRatio = gltfPerspectiveCamera->find("aspectRatio"_s);
        if(gltfAspectRatio) {
            if(!_d->gltf->parseFloat(*gltfAspectRatio)) {
                Error{} << "Trade::GltfImporter::camera(): invalid perspective aspectRatio property";
                return {};
            }
            if(gltfAspectRatio->asFloat() <= 0.0f) {
                Error{} << "Trade::GltfImporter::camera(): expected positive perspective aspectRatio, got" << gltfAspectRatio->asFloat();
                return {};
            }
        }

        const Utility::JsonToken* const gltfYfov = gltfPerspectiveCamera->find("yfov"_s);
        if(!gltfYfov || !_d->gltf->parseFloat(*gltfYfov)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid perspective yfov property";
            return {};
        }
        if(gltfYfov->asFloat() <= 0.0f) {
            Error{} << "Trade::GltfImporter::camera(): expected positive perspective yfov, got" << gltfYfov->asFloat();
            return {};
        }

        const Utility::JsonToken* const gltfZnear = gltfPerspectiveCamera->find("znear"_s);
        if(!gltfZnear || !_d->gltf->parseFloat(*gltfZnear)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid perspective znear property";
            return {};
        }
        if(gltfZnear->asFloat() <= 0.0f) {
            Error{} << "Trade::GltfImporter::camera(): expected positive perspective znear, got" << gltfZnear->asFloat();
            return {};
        }

        /* Z far is optional, if not set it's infinity (and yes, JSON has no
           way to represent an infinity, FFS) */
        const Utility::JsonToken* const gltfZfar = gltfPerspectiveCamera->find("zfar"_s);
        if(gltfZfar) {
            if(!_d->gltf->parseFloat(*gltfZfar)) {
                Error{} << "Trade::GltfImporter::camera(): invalid perspective zfar property";
                return {};
            }
            if(gltfZfar->asFloat() <= gltfZnear->asFloat()) {
                Error{} << "Trade::GltfImporter::camera(): expected perspective zfar larger than znear of" << gltfZnear->asFloat() << Debug::nospace << ", got" << gltfZfar->asFloat();
                return {};
            }
        }

        const Float aspectRatio = gltfAspectRatio ? gltfAspectRatio->asFloat() : 1.0f;
        /* glTF uses vertical FoV and X/Y aspect ratio, so to avoid accidental
           bugs we will directly calculate the near plane size and use that to
           create the camera data (instead of passing it the horizontal FoV) */
        const Vector2 size = 2.0f*gltfZnear->asFloat()*Math::tan(gltfYfov->asFloat()*0.5_radf)*Vector2::xScale(aspectRatio);
        const Float far = gltfZfar ? gltfZfar->asFloat() : Constants::inf();
        return CameraData{CameraType::Perspective3D, size, gltfZnear->asFloat(), far, &gltfCamera};
    }

    /* Orthographic camera */
    if(gltfType->asString() == "orthographic"_s) {
        const Utility::JsonToken* const gltfOrthographicCamera = gltfCamera.find("orthographic"_s);
        if(!gltfOrthographicCamera || !_d->gltf->parseObject(*gltfOrthographicCamera)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid orthographic property";
            return {};
        }

        const Utility::JsonToken* const gltfXmag = gltfOrthographicCamera->find("xmag"_s);
        if(!gltfXmag || !_d->gltf->parseFloat(*gltfXmag)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid orthographic xmag property";
            return {};
        }
        if(gltfXmag->asFloat() == 0.0f) {
            Error{} << "Trade::GltfImporter::camera(): expected non-zero orthographic xmag";
            return {};
        }

        const Utility::JsonToken* const gltfYmag = gltfOrthographicCamera->find("ymag"_s);
        if(!gltfYmag || !_d->gltf->parseFloat(*gltfYmag)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid orthographic ymag property";
            return {};
        }
        if(gltfYmag->asFloat() == 0.0f) {
            Error{} << "Trade::GltfImporter::camera(): expected non-zero orthographic ymag";
            return {};
        }

        const Utility::JsonToken* const gltfZnear = gltfOrthographicCamera->find("znear"_s);
        if(!gltfZnear || !_d->gltf->parseFloat(*gltfZnear)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid orthographic znear property";
            return {};
        }
        if(gltfZnear->asFloat() < 0.0f) {
            Error{} << "Trade::GltfImporter::camera(): expected non-negative orthographic znear, got" << gltfZnear->asFloat();
            return {};
        }

        const Utility::JsonToken* const gltfZfar = gltfOrthographicCamera->find("zfar"_s);
        if(!gltfZfar || !_d->gltf->parseFloat(*gltfZfar)) {
            Error{} << "Trade::GltfImporter::camera(): missing or invalid orthographic zfar property";
            return {};
        }
        if(gltfZfar->asFloat() <= gltfZnear->asFloat()) {
            Error{} << "Trade::GltfImporter::camera(): expected orthographic zfar larger than znear of" << gltfZnear->asFloat() << Debug::nospace << ", got" << gltfZfar->asFloat();
            return {};
        }

        return CameraData{CameraType::Orthographic3D,
            /* glTF uses a "scale" instead of "size", which means we have to
               double */
            Vector2{gltfXmag->asFloat(), gltfYmag->asFloat()}*2.0f,
            gltfZnear->asFloat(), gltfZfar->asFloat(), &gltfCamera};
    }

    Error{} << "Trade::GltfImporter::camera(): unrecognized type" << gltfType->asString();
    return {};
}

UnsignedInt GltfImporter::doLightCount() const {
    return _d->gltfLights.size();
}

Int GltfImporter::doLightForName(const Containers::StringView name) {
    if(!_d->lightsForName) {
        _d->lightsForName.emplace();
        _d->lightsForName->reserve(_d->gltfLights.size());
        for(std::size_t i = 0; i != _d->gltfLights.size(); ++i)
            if(const Containers::StringView n = _d->gltfLights[i].second())
                _d->lightsForName->emplace(n, i);
    }

    const auto found = _d->lightsForName->find(name);
    return found == _d->lightsForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doLightName(const UnsignedInt id) {
    return _d->gltfLights[id].second();
}

Containers::Optional<LightData> GltfImporter::doLight(const UnsignedInt id) {
    const Utility::JsonToken& gltfLight = _d->gltfLights[id].first();

    /* Color is optional, vector of 1.0 is a default if not set */
    Color3 color{1.0f};
    if(const Utility::JsonToken* const gltfColor = gltfLight.find("color"_s)) {
        const Containers::Optional<Containers::StridedArrayView1D<const float>> colorArray = _d->gltf->parseFloatArray(*gltfColor, 3);
        if(!colorArray) {
            Error{} << "Trade::GltfImporter::light(): invalid color property";
            return {};
        }

        Utility::copy(*colorArray, color.data());
    }

    /* Intensity is optional, 1.0 is a default if not set */
    const Utility::JsonToken* const gltfIntensity = gltfLight.find("intensity"_s);
    if(gltfIntensity && !_d->gltf->parseFloat(*gltfIntensity)) {
        Error{} << "Trade::GltfImporter::light(): invalid intensity property";
        return {};
    }

    /* Range is optional, infinity is a default if not set (and yes, JSON has
       no way to represent an infinity, FFS) */
    const Utility::JsonToken* const gltfRange = gltfLight.find("range"_s);
    if(gltfRange) {
        if(!_d->gltf->parseFloat(*gltfRange)) {
            Error{} << "Trade::GltfImporter::light(): invalid range property";
            return {};
        }
        if(gltfRange->asFloat() <= 0.0f) {
            Error{} << "Trade::GltfImporter::light(): expected positive range, got" << gltfRange->asFloat();
            return {};
        }
    }

    const Utility::JsonToken* const gltfType = gltfLight.find("type"_s);
    if(!gltfType || !_d->gltf->parseString(*gltfType)) {
        Error{} << "Trade::GltfImporter::light(): missing or invalid type property";
        return {};
    }

    /* Light type */
    LightData::Type type;
    if(gltfType->asString() == "point"_s) {
        type = LightData::Type::Point;
    } else if(gltfType->asString() == "spot"_s) {
        type = LightData::Type::Spot;
    } else if(gltfType->asString() == "directional"_s) {
        type = LightData::Type::Directional;
    } else {
        Error{} << "Trade::GltfImporter::light(): unrecognized type" << gltfType->asString();
        return {};
    }

    /* Spotlight cone angles. In glTF they're specified as half-angles (which
       is also why the limit on outer angle is 90°, not 180°), to avoid
       confusion report a potential error in the original half-angles and
       double the angle only at the end. */
    Rad innerConeAngle{NoInit}, outerConeAngle{NoInit};
    if(type == LightData::Type::Spot) {
        innerConeAngle = 0.0_degf;
        outerConeAngle = 45.0_degf;

        const Utility::JsonToken* const gltfSpot = gltfLight.find("spot"_s);
        if(!gltfSpot || !_d->gltf->parseObject(*gltfSpot)) {
            Error{} << "Trade::GltfImporter::light(): missing or invalid spot property";
            return {};
        }

        if(const Utility::JsonToken* const gltfInnerConeAngle = gltfSpot->find("innerConeAngle"_s)) {
            const Containers::Optional<Float> angle = _d->gltf->parseFloat(*gltfInnerConeAngle);
            if(!angle) {
                Error{} << "Trade::GltfImporter::light(): invalid spot innerConeAngle property";
                return {};
            }

            innerConeAngle = Rad{*angle};
        }

        if(const Utility::JsonToken* const gltfOuterConeAngle = gltfSpot->find("outerConeAngle"_s)) {
            const Containers::Optional<Float> angle = _d->gltf->parseFloat(*gltfOuterConeAngle);
            if(!angle) {
                Error{} << "Trade::GltfImporter::light(): invalid spot outerConeAngle property";
                return {};
            }

            outerConeAngle = Rad{*angle};
        }

        if(innerConeAngle < Rad(0.0_degf) || innerConeAngle >= outerConeAngle || outerConeAngle >= Rad(90.0_degf)) {
            Error{} << "Trade::GltfImporter::light(): spot inner and outer cone angle" << Deg(innerConeAngle) << "and" << Deg(outerConeAngle) << "out of allowed bounds";
            return {};
        }
    } else innerConeAngle = outerConeAngle = 180.0_degf;

    /* Range should be infinity for directional lights. Because there's no way
       to represent infinity in JSON, directly suggest to remove the range
       property, don't even bother printing the value. */
    if(type == LightData::Type::Directional && gltfRange) {
        Error{} << "Trade::GltfImporter::light(): range can't be defined for a directional light";
        return {};
    }

    /* As said above, glTF uses half-angles, while we have full angles (for
       consistency with existing APIs such as OpenAL cone angles or math intersection routines as well as Blender). */
    return LightData{type, color,
        gltfIntensity ? gltfIntensity->asFloat() : 1.0f,
        gltfRange ? gltfRange->asFloat() : Constants::inf(),
        innerConeAngle*2.0f, outerConeAngle*2.0f, &gltfLight};
}

Int GltfImporter::doDefaultScene() const {
    if(const Utility::JsonToken* gltfScene = _d->gltf->root().find("scene"_s)) {
        /* All checking and parsing was done in doOpenData() already, as this
           function is not allowed to fail */
        return gltfScene->asUnsignedInt();
    }

    /* While https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#scenes
       says that "When scene is undefined, client implementations MAY delay
       rendering until a particular scene is requested.", several official
       sample glTF models (e.g. the AnimatedTriangle) have no "scene" property,
       so that's a bit stupid behavior to have. As per discussion at
       https://github.com/KhronosGroup/glTF/issues/815#issuecomment-274286889,
       if a default scene isn't defined and there is at least one scene, just
       use the first one. */
    return _d->gltfScenes.isEmpty() ? -1 : 0;
}

UnsignedInt GltfImporter::doSceneCount() const {
    return _d->gltfScenes.size();
}

Int GltfImporter::doSceneForName(const Containers::StringView name) {
    if(!_d->scenesForName) {
        _d->scenesForName.emplace();
        _d->scenesForName->reserve(_d->gltfScenes.size());
        for(std::size_t i = 0; i != _d->gltfScenes.size(); ++i) {
            if(const Containers::StringView n = _d->gltfScenes[i].second())
                _d->scenesForName->emplace(n, i);
        }
    }

    const auto found = _d->scenesForName->find(name);
    return found == _d->scenesForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doSceneName(const UnsignedInt id) {
    return _d->gltfScenes[id].second();
}

Containers::Optional<SceneData> GltfImporter::doScene(UnsignedInt id) {
    const Utility::JsonToken& gltfScene = _d->gltfScenes[id].first();

    /* Gather all top-level nodes belonging to a scene and recursively populate
       the children ranges. Optimistically assume the glTF has just a single
       scene and reserve for that. */
    /** @todo once we have BitArrays use the objects array to mark nodes that
        are present in the scene and then create a new array from those but
        ordered so we can have OrderedMapping for parents and also all other
        fields */
    Containers::Array<UnsignedInt> objects;
    arrayReserve(objects, _d->gltfNodes.size());
    if(const Utility::JsonToken* const gltfSceneNodes = gltfScene.find("nodes"_s)) {
        /* Scene node array parsed in doOpenData() already, for cycle
           detection. Bounds checked there as well, so we can just directly
           copy the contents. */
        const Containers::StridedArrayView1D<const UnsignedInt> sceneNodes = gltfSceneNodes->asUnsignedIntArray();
        Utility::copy(sceneNodes, arrayAppend(objects, NoInit, sceneNodes.size()));
    }

    /* Offset array, `children[i + 1]` to `children[i + 2]` defines a range in
       `objects` containing children of object `i`, `children[0]` to
       `children[1]` is the range of root objects with `children[0]` being
       always `0` */
    Containers::Array<UnsignedInt> children;
    arrayReserve(children, _d->gltfNodes.size() + 2);
    arrayAppend(children, {0u, UnsignedInt(objects.size())});
    for(std::size_t i = 0; i != children.size() - 1; ++i) {
        for(std::size_t j = children[i], jMax = children[i + 1]; j != jMax; ++j) {
            const Utility::JsonToken& gltfNode = _d->gltfNodes[objects[j]].first();
            if(const Utility::JsonToken* const gltfNodeChildren = gltfNode.find("children"_s)) {
                /* Node children array parsed in doOpenData() already, for
                   cycle detection. Bounds checked there as well, so we can
                   just directly copy the contents. */
                const Containers::StridedArrayView1D<const UnsignedInt> nodeChildren = gltfNodeChildren->asUnsignedIntArray();
                Utility::copy(nodeChildren, arrayAppend(objects, NoInit, nodeChildren.size()));
            }
            arrayAppend(children, UnsignedInt(objects.size()));
        }
    }

    /** @todo once there's SceneData::mappingRange(), calculate also min here */
    const UnsignedInt maxObjectIndexPlusOne = objects.isEmpty() ? 0 : Math::max(objects) + 1;

    /* Count how many objects have matrices, how many have separate TRS
       properties and which of the set are present. Then also gather mesh,
       light, camera and skin assignment count. Materials have to use the same
       object mapping as meshes, so only check if there's any material
       assignment at all -- if not, then we won't need to store that field. */
    UnsignedInt transformationCount = 0;
    UnsignedInt trsCount = 0;
    bool hasTranslations = false;
    bool hasRotations = false;
    bool hasScalings = false;
    UnsignedInt meshCount = 0;
    bool hasMeshMaterials = false;
    UnsignedInt lightCount = 0;
    UnsignedInt cameraCount = 0;
    UnsignedInt skinCount = 0;
    for(const UnsignedInt i: objects) {
        const Utility::JsonToken& gltfNode = _d->gltfNodes[i].first();

        /* Cache repeated queries to not suffer from the O(n) lookup too much */
        const bool hasTranslation = gltfNode.find("translation"_s);
        const bool hasRotation = gltfNode.find("rotation"_s);
        const bool hasScaling = gltfNode.find("scale"_s);

        /* Everything that has a TRS should have a transformation matrix as
           well. OTOH there can be a transformation matrix but no TRS, and
           there can also be objects without any transformation. */
        if(hasTranslation ||
           hasRotation ||
           hasScaling) {
            ++trsCount;
            ++transformationCount;
        } else if(gltfNode.find("matrix"_s)) ++transformationCount;

        if(hasTranslation) hasTranslations = true;
        if(hasRotation) hasRotations = true;
        if(hasScaling) hasScalings = true;

        /* Mesh reference */
        if(const Utility::JsonToken* const gltfMesh = gltfNode.find("mesh"_s)) {
            if(!_d->gltf->parseUnsignedInt(*gltfMesh)) {
                Error{} << "Trade::GltfImporter::scene(): invalid mesh property of node" << i;
                return {};
            }
            const UnsignedInt mesh = gltfMesh->asUnsignedInt();
            if(mesh >= _d->gltfMeshes.size()) {
                Error{} << "Trade::GltfImporter::scene(): mesh index" << mesh << "in node" << i << "out of range for" << _d->gltfMeshes.size() << "meshes";
                return {};
            }

            meshCount += _d->meshSizeOffsets[mesh + 1] - _d->meshSizeOffsets[mesh];
            for(std::size_t j = _d->meshSizeOffsets[mesh], jMax = _d->meshSizeOffsets[mesh + 1]; j != jMax; ++j) {
                if(const Utility::JsonToken* gltfPrimitiveMaterial = _d->gltfMeshPrimitiveMap[j].second()->find("material"_s)) {
                    if(!_d->gltf->parseUnsignedInt(*gltfPrimitiveMaterial)) {
                        Error{} << "Trade::GltfImporter::scene(): invalid material property of mesh" << mesh << "primitive" << j - _d->meshSizeOffsets[mesh];
                        return {};
                    }
                    if(gltfPrimitiveMaterial->asUnsignedInt() >= _d->gltfMaterials.size()) {
                        Error{} << "Trade::GltfImporter::scene(): material index" << gltfPrimitiveMaterial->asUnsignedInt() << "in mesh" << mesh << "primitive" << j - _d->meshSizeOffsets[mesh] << "out of range for" << _d->gltfMaterials.size() << "materials";
                        return {};
                    }

                    hasMeshMaterials = true;
                    /* No break here to ensure parsing and checks are is called
                       on materials of all primitives */
                }
            }
        }

        /* Camera reference */
        if(const Utility::JsonToken* const gltfCamera = gltfNode.find("camera"_s)) {
            if(!_d->gltf->parseUnsignedInt(*gltfCamera)) {
                Error{} << "Trade::GltfImporter::scene(): invalid camera property of node" << i;
                return {};
            }
            if(gltfCamera->asUnsignedInt() >= _d->gltfCameras.size()) {
                Error{} << "Trade::GltfImporter::scene(): camera index" << gltfCamera->asUnsignedInt() << "in node" << i << "out of range for" << _d->gltfCameras.size() << "cameras";
                return {};
            }

            ++cameraCount;
        }

        /* Skin reference */
        if(const Utility::JsonToken* const gltfSkin = gltfNode.find("skin"_s)) {
            if(!_d->gltf->parseUnsignedInt(*gltfSkin)) {
                Error{} << "Trade::GltfImporter::scene(): invalid skin property of node" << i;
                return {};
            }
            if(gltfSkin->asUnsignedInt() >= _d->gltfSkins.size()) {
                Error{} << "Trade::GltfImporter::scene(): skin index" << gltfSkin->asUnsignedInt() << "in node" << i << "out of range for" << _d->gltfSkins.size() << "skins";
                return {};
            }

            ++skinCount;
        }

        /* Extensions */
        if(const Utility::JsonToken* const gltfExtensions = gltfNode.find("extensions"_s)) {
            if(!_d->gltf->parseObject(*gltfExtensions)) {
                Error{} << "Trade::GltfImporter::scene(): invalid node" << i << "extensions property";
                return {};
            }

            /* Light reference */
            if(const Utility::JsonToken* const gltfKhrLightsPunctual = gltfExtensions->find("KHR_lights_punctual"_s)) {
                if(!_d->gltf->parseObject(*gltfKhrLightsPunctual)) {
                    Error{} << "Trade::GltfImporter::scene(): invalid node" << i << "KHR_lights_punctual extension";
                    return {};
                }

                const Utility::JsonToken* gltfLight = gltfKhrLightsPunctual->find("light"_s);
                if(!gltfLight || !_d->gltf->parseUnsignedInt(*gltfLight)) {
                    Error{} << "Trade::GltfImporter::scene(): missing or invalid KHR_lights_punctual light property of node" << i;
                    return {};
                }
                if(gltfLight->asUnsignedInt() >= _d->gltfLights.size()) {
                    Error{} << "Trade::GltfImporter::scene(): light index" << gltfLight->asUnsignedInt() << "in node" << i << "out of range for" << _d->gltfLights.size() << "lights";
                    return {};
                }

                ++lightCount;
            }
        }
    }

    /* If all objects that have transformations have TRS as well, no need to
       store the combined transform field */
    if(trsCount == transformationCount) transformationCount = 0;

    /* Allocate the output array */
    Containers::ArrayView<UnsignedInt> parentImporterStateObjects;
    Containers::ArrayView<Int> parents;
    Containers::ArrayView<const Utility::JsonToken*> importerState;
    Containers::ArrayView<UnsignedInt> transformationObjects;
    Containers::ArrayView<Matrix4> transformations;
    Containers::ArrayView<UnsignedInt> trsObjects;
    Containers::ArrayView<Vector3> translations;
    Containers::ArrayView<Quaternion> rotations;
    Containers::ArrayView<Vector3> scalings;
    Containers::ArrayView<UnsignedInt> meshMaterialObjects;
    Containers::ArrayView<UnsignedInt> meshes;
    Containers::ArrayView<Int> meshMaterials;
    Containers::ArrayView<UnsignedInt> lightObjects;
    Containers::ArrayView<UnsignedInt> lights;
    Containers::ArrayView<UnsignedInt> cameraObjects;
    Containers::ArrayView<UnsignedInt> cameras;
    Containers::ArrayView<UnsignedInt> skinObjects;
    Containers::ArrayView<UnsignedInt> skins;
    Containers::Array<char> data = Containers::ArrayTuple{
        {NoInit, objects.size(), parentImporterStateObjects},
        {NoInit, objects.size(), parents},
        {NoInit, objects.size(), importerState},
        {NoInit, transformationCount, transformationObjects},
        {NoInit, transformationCount, transformations},
        {NoInit, trsCount, trsObjects},
        {NoInit, hasTranslations ? trsCount : 0, translations},
        {NoInit, hasRotations ? trsCount : 0, rotations},
        {NoInit, hasScalings ? trsCount : 0, scalings},
        {NoInit, meshCount, meshMaterialObjects},
        {NoInit, meshCount, meshes},
        {NoInit, hasMeshMaterials ? meshCount : 0, meshMaterials},
        {NoInit, lightCount, lightObjects},
        {NoInit, lightCount, lights},
        {NoInit, cameraCount, cameraObjects},
        {NoInit, cameraCount, cameras},
        {NoInit, skinCount, skinObjects},
        {NoInit, skinCount, skins}
    };

    /* Populate object mapping for parents and importer state, synthesize
       parent info from the child ranges */
    Utility::copy(objects, parentImporterStateObjects);
    for(std::size_t i = 0; i != children.size() - 1; ++i) {
        Int parent = Int(i) - 1;
        for(std::size_t j = children[i], jMax = children[i + 1]; j != jMax; ++j)
            parents[j] = parent == -1 ? -1 : objects[parent];
    }

    /* Populate the rest */
    std::size_t transformationOffset = 0;
    std::size_t trsOffset = 0;
    std::size_t meshMaterialOffset = 0;
    std::size_t lightOffset = 0;
    std::size_t cameraOffset = 0;
    std::size_t skinOffset = 0;
    for(std::size_t i = 0; i != objects.size(); ++i) {
        const UnsignedInt nodeI = objects[i];
        const Utility::JsonToken& gltfNode = _d->gltfNodes[nodeI].first();

        /* Populate importer state */
        importerState[i] = &gltfNode;

        /* Parse TRS */
        Vector3 translation;
        const Utility::JsonToken* const gltfTranslation = gltfNode.find("translation"_s);
        if(gltfTranslation) {
            const Containers::Optional<Containers::StridedArrayView1D<const float>> translationArray = _d->gltf->parseFloatArray(*gltfTranslation, 3);
            if(!translationArray) {
                Error{} << "Trade::GltfImporter::scene(): invalid translation property of node" << nodeI;
                return {};
            }

            Utility::copy(*translationArray, translation.data());
        }

        Quaternion rotation;
        const Utility::JsonToken* const gltfRotation = gltfNode.find("rotation"_s);
        if(gltfRotation) {
            const Containers::Optional<Containers::StridedArrayView1D<const float>> rotationArray = _d->gltf->parseFloatArray(*gltfRotation, 4);
            if(!rotationArray) {
                Error{} << "Trade::GltfImporter::scene(): invalid rotation property of node" << nodeI;
                return {};
            }

            /* glTF also uses the XYZW order */
            Utility::copy(*rotationArray, rotation.data());
            if(!rotation.isNormalized() && configuration().value<bool>("normalizeQuaternions")) {
                rotation = rotation.normalized();
                Warning{} << "Trade::GltfImporter::scene(): rotation quaternion of node" << nodeI << "was renormalized";
            }
        }

        Vector3 scaling{1.0f};
        const Utility::JsonToken* const gltfScale = gltfNode.find("scale"_s);
        if(gltfScale) {
            const Containers::Optional<Containers::StridedArrayView1D<const float>> scalingArray = _d->gltf->parseFloatArray(*gltfScale, 3);
            if(!scalingArray) {
                Error{} << "Trade::GltfImporter::scene(): invalid scale property of node" << nodeI;
                return {};
            }

            Utility::copy(*scalingArray, scaling.data());
        }

        /* Parse transformation, or combine it from TRS if not present */
        Matrix4 transformation;
        const Utility::JsonToken* const gltfMatrix = gltfNode.find("matrix"_s);
        if(gltfMatrix) {
            const Containers::Optional<Containers::StridedArrayView1D<const float>> transformationArray = _d->gltf->parseFloatArray(*gltfMatrix, 16);
            if(!transformationArray) {
                Error{} << "Trade::GltfImporter::scene(): invalid matrix property of node" << nodeI;
                return {};
            }

            Utility::copy(*transformationArray, transformation.data());
        } else transformation =
            Matrix4::translation(translation)*
            Matrix4{rotation.toMatrix()}*
            Matrix4::scaling(scaling);

        /* Populate the combined transformation and object mapping only if
           there's actually some transformation for this object and we want to
           store it -- if all objects have TRS anyway, the matrix is redundant */
        if((gltfMatrix ||
            gltfTranslation ||
            gltfRotation ||
            gltfScale) && transformationCount)
        {
            transformations[transformationOffset] = transformation;
            transformationObjects[transformationOffset] = nodeI;
            ++transformationOffset;
        }

        /* Store the TRS information and object mapping only if there was
           something */
        if(gltfTranslation ||
           gltfRotation ||
           gltfScale)
        {
            if(hasTranslations) translations[trsOffset] = translation;
            if(hasRotations) rotations[trsOffset] = rotation;
            if(hasScalings) scalings[trsOffset] = scaling;
            trsObjects[trsOffset] = nodeI;
            ++trsOffset;
        }

        /* Populate mesh references. All parsing and bounds checks done in the
           previous pass already. */
        if(const Utility::JsonToken* const gltfMesh = gltfNode.find("mesh"_s)) {
            const UnsignedInt mesh = gltfMesh->asUnsignedInt();
            for(std::size_t j = _d->meshSizeOffsets[mesh], jMax = _d->meshSizeOffsets[mesh + 1]; j != jMax; ++j) {
                meshMaterialObjects[meshMaterialOffset] = nodeI;
                meshes[meshMaterialOffset] = j;
                if(const Utility::JsonToken* gltfPrimitiveMaterial = _d->gltfMeshPrimitiveMap[j].second()->find("material"_s)) {
                    meshMaterials[meshMaterialOffset] = gltfPrimitiveMaterial->asUnsignedInt();
                } else if(hasMeshMaterials)
                    meshMaterials[meshMaterialOffset] = -1;
                ++meshMaterialOffset;
            }
        }

        /* Populate camera references. Parsing and bounds check done in the
           previous pass already. */
        if(const Utility::JsonToken* const gltfCamera = gltfNode.find("camera"_s)) {
            cameraObjects[cameraOffset] = nodeI;
            cameras[cameraOffset] = gltfCamera->asUnsignedInt();
            ++cameraOffset;
        }

        /* Populate skin references. Parsing and bounds check done in the
           previous pass already. */
        if(const Utility::JsonToken* const gltfSkin = gltfNode.find("skin"_s)) {
            skinObjects[skinOffset] = nodeI;
            skins[skinOffset] = gltfSkin->asUnsignedInt();
            ++skinOffset;
        }

        /* Extensions. Type of the property checked in the previous pass
           already. */
        if(const Utility::JsonToken* const gltfExtensions = gltfNode.find("extensions"_s)) {
            /* Populate light references. Property type, parsing and bounds
               check done in the previous pass already. */
            if(const Utility::JsonToken* const gltfKhrLightsPunctual = gltfExtensions->find("KHR_lights_punctual"_s)) {
                lightObjects[lightOffset] = nodeI;
                lights[lightOffset] = (*gltfKhrLightsPunctual)["light"_s].asUnsignedInt();
                ++lightOffset;
            }
        }
    }

    CORRADE_INTERNAL_ASSERT(
        transformationOffset == transformations.size() &&
        trsOffset == trsObjects.size() &&
        meshMaterialOffset == meshMaterialObjects.size() &&
        lightOffset == lightObjects.size() &&
        cameraOffset == cameraObjects.size() &&
        skinOffset == skinObjects.size());

    /* Put everything together. For simplicity the imported data could always
       have all fields present, with some being empty, but this gives less
       noise for asset introspection purposes. */
    Containers::Array<SceneFieldData> fields;
    arrayAppend(fields, {
        /** @todo once there's a flag to annotate implicit fields, omit the
            parent field if it's all -1s; or alternatively we could also have a
            stride of 0 for this case */
        SceneFieldData{SceneField::Parent, parentImporterStateObjects, parents},
        SceneFieldData{SceneField::ImporterState, parentImporterStateObjects, importerState}
    });

    /* Transformations. If there's no such field, add an empty transformation
       to indicate it's a 3D scene. */
    if(transformationCount) arrayAppend(fields, SceneFieldData{
        SceneField::Transformation, transformationObjects, transformations
    });
    if(hasTranslations) arrayAppend(fields, SceneFieldData{
        SceneField::Translation, trsObjects, translations
    });
    if(hasRotations) arrayAppend(fields, SceneFieldData{
        SceneField::Rotation, trsObjects, rotations
    });
    if(hasScalings) arrayAppend(fields, SceneFieldData{
        SceneField::Scaling, trsObjects, scalings
    });
    if(!transformationCount && !trsCount) arrayAppend(fields, SceneFieldData{
        SceneField::Transformation, SceneMappingType::UnsignedInt, nullptr, SceneFieldType::Matrix4x4, nullptr
    });

    if(meshCount) arrayAppend(fields, SceneFieldData{
        SceneField::Mesh, meshMaterialObjects, meshes
    });
    if(hasMeshMaterials) arrayAppend(fields, SceneFieldData{
        SceneField::MeshMaterial, meshMaterialObjects, meshMaterials
    });
    if(lightCount) arrayAppend(fields, SceneFieldData{
        SceneField::Light, lightObjects, lights
    });
    if(cameraCount) arrayAppend(fields, SceneFieldData{
        SceneField::Camera, cameraObjects, cameras
    });
    if(skinCount) arrayAppend(fields, SceneFieldData{
        SceneField::Skin, skinObjects, skins
    });

    /* Convert back to the default deleter to avoid dangling deleter function
       pointer issues when unloading the plugin */
    arrayShrink(fields, DefaultInit);
    /* Even though SceneData is capable of holding more than 4 billion objects,
       we realistically don't expect glTF to have that many -- the text file
       would be *terabytes* then */
    return SceneData{SceneMappingType::UnsignedInt, maxObjectIndexPlusOne, std::move(data), std::move(fields), &gltfScene};
}

UnsignedLong GltfImporter::doObjectCount() const {
    return _d->gltfNodes.size();
}

Long GltfImporter::doObjectForName(const Containers::StringView name) {
    if(!_d->nodesForName) {
        _d->nodesForName.emplace();
        _d->nodesForName->reserve(_d->gltfNodes.size());
        for(std::size_t i = 0; i != _d->gltfNodes.size(); ++i) {
            if(const Containers::StringView n = _d->gltfNodes[i].second())
                _d->nodesForName->emplace(n, i);
        }
    }

    const auto found = _d->nodesForName->find(name);
    return found == _d->nodesForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doObjectName(const UnsignedLong id) {
    return _d->gltfNodes[id].second();
}

UnsignedInt GltfImporter::doSkin3DCount() const {
    return _d->gltfSkins.size();
}

Int GltfImporter::doSkin3DForName(const Containers::StringView name) {
    if(!_d->skinsForName) {
        _d->skinsForName.emplace();
        _d->skinsForName->reserve(_d->gltfSkins.size());
        for(std::size_t i = 0; i != _d->gltfSkins.size(); ++i)
            if(const Containers::StringView n = _d->gltfSkins[i].second())
                _d->skinsForName->emplace(n, i);
    }

    const auto found = _d->skinsForName->find(name);
    return found == _d->skinsForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doSkin3DName(const UnsignedInt id) {
    return _d->gltfSkins[id].second();
}

Containers::Optional<SkinData3D> GltfImporter::doSkin3D(const UnsignedInt id) {
    const Utility::JsonToken& gltfSkin = _d->gltfSkins[id].first();

    /* Joint IDs */
    const Utility::JsonToken* const gltfJoints = gltfSkin.find("joints"_s);
    Containers::Optional<Containers::StridedArrayView1D<const UnsignedInt>> jointsArray;
    if(!gltfJoints || !(jointsArray = _d->gltf->parseUnsignedIntArray(*gltfJoints))) {
        Error{} << "Trade::GltfImporter::skin3D(): missing or invalid joints property";
        return {};
    }
    if(jointsArray->isEmpty()) {
        Error{} << "Trade::GltfImporter::skin3D(): skin has no joints";
        return {};
    }
    Containers::Array<UnsignedInt> joints{NoInit, jointsArray->size()};
    for(std::size_t i = 0; i != jointsArray->size(); ++i) {
        const UnsignedInt joint = (*jointsArray)[i];
        if(joint >= _d->gltfNodes.size()) {
            Error{} << "Trade::GltfImporter::skin3D(): joint index" << joint << "out of range for" << _d->gltfNodes.size() << "nodes";
            return {};
        }

        joints[i] = joint;
    }

    /* Inverse bind matrices. If there are none, default is identities */
    Containers::Array<Matrix4> inverseBindMatrices{ValueInit, joints.size()};
    if(const Utility::JsonToken* const gltfInverseBindMatrices = gltfSkin.find("inverseBindMatrices"_s)) {
        if(!_d->gltf->parseUnsignedInt(*gltfInverseBindMatrices)) {
            Error{} << "Trade::GltfImporter::skin3D(): invalid inverseBindMatrices property";
            return {};
        }

        const Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>> accessor = parseAccessor("Trade::GltfImporter::skin3D():", gltfInverseBindMatrices->asUnsignedInt());
        if(!accessor)
            return {};
        if(accessor->second() != VertexFormat::Matrix4x4) {
            /* Since we're abusing VertexFormat for all formats, print just the
               enum value without the prefix to avoid cofusion */
            Error{} << "Trade::GltfImporter::skin3D(): inverse bind matrices have unexpected type" << Debug::packed << accessor->second();
            return {};
        }

        Containers::StridedArrayView1D<const Matrix4> matrices = Containers::arrayCast<1, const Matrix4>(accessor->first());
        if(matrices.size() != inverseBindMatrices.size()) {
            Error{} << "Trade::GltfImporter::skin3D(): invalid inverse bind matrix count, expected" << inverseBindMatrices.size() << "but got" << matrices.size();
            return {};
        }

        Utility::copy(matrices, inverseBindMatrices);
    }

    return SkinData3D{std::move(joints), std::move(inverseBindMatrices), &gltfSkin};
}

UnsignedInt GltfImporter::doMeshCount() const {
    return _d->gltfMeshPrimitiveMap.size();
}

Int GltfImporter::doMeshForName(const Containers::StringView name) {
    /* As we can't fail here, name strings were parsed during import already
       (with the assumption they're mostly not escaped and thus overhead-less),
       but the map is populated lazily as that *is* some work */
    if(!_d->meshesForName) {
        _d->meshesForName.emplace();
        _d->meshesForName->reserve(_d->gltfMeshes.size());
        for(std::size_t i = 0; i != _d->gltfMeshes.size(); ++i) {
            if(const Containers::StringView n = _d->gltfMeshes[i].second())
                /* The mesh can be duplicated for as many primitives as it has,
                   point to the first mesh in the duplicate sequence */
                _d->meshesForName->emplace(n, _d->meshSizeOffsets[i]);
        }
    }

    const auto found = _d->meshesForName->find(name);
    return found == _d->meshesForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doMeshName(const UnsignedInt id) {
    /* This returns the same name for all multi-primitive mesh duplicates */
    return _d->gltfMeshes[_d->gltfMeshPrimitiveMap[id].first()].second();
}

namespace {

/* Used in doMesh() and doMaterial() to remove duplicate keys from a JSON
   object. For consistent behavior across all STL implementation it uses a
   stable sort, thus preserving the order of duplicates. Then, all duplicates
   except the last one are removed, consistently with what cgltf or json.hpp
   does. */
/** @todo drop "all except last" and use only the first, as that's what the
    Utility::JsonToken::find() do */
template<class T, class F, class G> std::size_t stableSortRemoveDuplicatesToPrefix(Containers::ArrayView<T> container, F lessThanComparator, G equalComparator) {
    std::stable_sort(container.begin(), container.end(), lessThanComparator);
    const auto reversed = stridedArrayView(container).template flipped<0>();
    return std::unique(reversed.begin(), reversed.end(), equalComparator) - reversed.begin();
}

}

Containers::Optional<MeshData> GltfImporter::doMesh(const UnsignedInt id, UnsignedInt) {
    const Utility::JsonToken& gltfPrimitive = _d->gltfMeshPrimitiveMap[id].second();

    /* Primitive is optional, defaulting to triangles */
    MeshPrimitive primitive = MeshPrimitive::Triangles;
    if(const Utility::JsonToken* gltfMode = gltfPrimitive.find("mode"_s)) {
        if(!_d->gltf->parseUnsignedInt(*gltfMode)) {
            Error{} << "Trade::GltfImporter::mesh(): invalid primitive mode property";
            return {};
        }
        switch(gltfMode->asUnsignedInt()) {
            case Implementation::GltfModePoints:
                primitive = MeshPrimitive::Points;
                break;
            case Implementation::GltfModeLines:
                primitive = MeshPrimitive::Lines;
                break;
            case Implementation::GltfModeLineLoop:
                primitive = MeshPrimitive::LineLoop;
                break;
            case Implementation::GltfModeLineStrip:
                primitive = MeshPrimitive::LineStrip;
                break;
            case Implementation::GltfModeTriangles:
                primitive = MeshPrimitive::Triangles;
                break;
            case Implementation::GltfModeTriangleStrip:
                primitive = MeshPrimitive::TriangleStrip;
                break;
            case Implementation::GltfModeTriangleFan:
                primitive = MeshPrimitive::TriangleFan;
                break;
            default:
                Error{} << "Trade::GltfImporter::mesh(): unrecognized primitive" << gltfMode->asUnsignedInt();
                return {};
        }
    }

    /* Attributes, if present. The glTF spec requires a primitive to define an
       attribute property with at least one attribute, but we allow without. */
    Containers::Array<Containers::Pair<Containers::StringView, UnsignedInt>> attributeOrder;
    if(const Utility::JsonToken* gltfAttributes = gltfPrimitive.find("attributes"_s)) {
        /* Primitive attributes object parsed in doOpenData() already, for
           custom attribute discovery, so we just use it directly. */
        for(Utility::JsonObjectItem gltfAttribute: gltfAttributes->asObject()) {
            if(!_d->gltf->parseUnsignedInt(gltfAttribute.value())) {
                Error{} << "Trade::GltfImporter::mesh(): invalid attribute" << gltfAttribute.key();
                return {};
            }
            /* Bounds check is done in parseAccessor() later, no need to do it
               here again */

            arrayAppend(attributeOrder, InPlaceInit, gltfAttribute.key(), gltfAttribute.value().asUnsignedInt());
        }
    }

    /* Sort and remove duplicates except the last one. Attributes sorted by
       name so that we add attribute sets in the correct order and can warn if
       indices are not contiguous. */
    const std::size_t uniqueAttributeCount = stableSortRemoveDuplicatesToPrefix(arrayView(attributeOrder),
        [](const Containers::Pair<Containers::StringView, UnsignedInt>& a, const Containers::Pair<Containers::StringView, UnsignedInt>& b) {
            return a.first() < b.first();
        },
        [](const Containers::Pair<Containers::StringView, UnsignedInt>& a, const Containers::Pair<Containers::StringView, UnsignedInt>& b) {
            return a.first() == b.first();
        });

    /* Gather all (whitelisted) attributes and the total buffer range spanning
       them */
    UnsignedInt bufferId = 0;
    UnsignedInt vertexCount = 0;
    std::size_t attributeId = 0;
    Containers::Pair<Containers::StringView, Int> lastNumberedAttribute;
    Math::Range1D<std::size_t> bufferRange;
    Containers::Array<MeshAttributeData> attributeData{uniqueAttributeCount};
    /** @todo use suffix() once it takes suffix size and not prefix size */
    for(const Containers::Pair<Containers::StringView, UnsignedInt>& attribute: attributeOrder.exceptPrefix(attributeOrder.size() - uniqueAttributeCount)) {
        /* Duplicate attribute, skip */
        if(attribute.second() == ~0u) continue;

        /* Extract base name and number from builtin glTF numbered attributes,
           use the whole name otherwise */
        Containers::StringView baseAttributeName;
        if(isBuiltinNumberedMeshAttribute(attribute.first())) {
            const Containers::Array3<Containers::StringView> attributeNameNumber = attribute.first().partition('_');
            /* Numbered attributes are expected to be contiguous (COLORS_0,
               COLORS_1...). If not, print a warning, because in the MeshData
               they will appear as contiguous. */
            if(lastNumberedAttribute.first() != attributeNameNumber[0])
                lastNumberedAttribute.second() = -1;
            const Int index = attributeNameNumber[2][0] - '0';
            if(index != lastNumberedAttribute.second() + 1) {
                Warning{} << "Trade::GltfImporter::mesh(): found attribute" << attribute.first() << "but expected" << attributeNameNumber[0] << Debug::nospace << "_" << Debug::nospace << lastNumberedAttribute.second() + 1;
            }

            baseAttributeName = attributeNameNumber[0];
            lastNumberedAttribute = {baseAttributeName, index};

        /* If not a builtin glTF numbered attribute or it was something strange
           such as TEXCOORD alone, TEXCOORD_SECOND, or currently also
           TEXCOORD_10, use the full attribute string. */
        } else {
            baseAttributeName = attribute.first();
            lastNumberedAttribute = {};
        }

        /* Get the accessor view */
        Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>> accessor = parseAccessor("Trade::GltfImporter::mesh():", attribute.second());
        if(!accessor) return {};

        /* Whitelist supported attribute and format combinations. If not
           allowed, name stays empty, which produces an error in a single place
           below. */
        MeshAttribute name{};
        if(baseAttributeName == "POSITION"_s) {
            if(accessor->second() == VertexFormat::Vector3 ||
               accessor->second() == VertexFormat::Vector3b ||
               accessor->second() == VertexFormat::Vector3bNormalized ||
               accessor->second() == VertexFormat::Vector3ub ||
               accessor->second() == VertexFormat::Vector3ubNormalized ||
               accessor->second() == VertexFormat::Vector3s ||
               accessor->second() == VertexFormat::Vector3sNormalized ||
               accessor->second() == VertexFormat::Vector3us ||
               accessor->second() == VertexFormat::Vector3usNormalized)
                name = MeshAttribute::Position;
        } else if(baseAttributeName == "NORMAL"_s) {
            if(accessor->second() == VertexFormat::Vector3 ||
               accessor->second() == VertexFormat::Vector3bNormalized ||
               accessor->second() == VertexFormat::Vector3sNormalized)
                name = MeshAttribute::Normal;
        } else if(baseAttributeName == "TANGENT"_s) {
            if(accessor->second() == VertexFormat::Vector4 ||
               accessor->second() == VertexFormat::Vector4bNormalized ||
               accessor->second() == VertexFormat::Vector4sNormalized)
                name = MeshAttribute::Tangent;
        } else if(baseAttributeName == "TEXCOORD"_s) {
            if(accessor->second() == VertexFormat::Vector2 ||
               accessor->second() == VertexFormat::Vector2b ||
               accessor->second() == VertexFormat::Vector2bNormalized ||
               accessor->second() == VertexFormat::Vector2ub ||
               accessor->second() == VertexFormat::Vector2ubNormalized ||
               accessor->second() == VertexFormat::Vector2s ||
               accessor->second() == VertexFormat::Vector2sNormalized ||
               accessor->second() == VertexFormat::Vector2us ||
               accessor->second() == VertexFormat::Vector2usNormalized)
                name = MeshAttribute::TextureCoordinates;
        } else if(baseAttributeName == "COLOR"_s) {
            if(accessor->second() == VertexFormat::Vector3 ||
               accessor->second() == VertexFormat::Vector4 ||
               accessor->second() == VertexFormat::Vector3ubNormalized ||
               accessor->second() == VertexFormat::Vector4ubNormalized ||
               accessor->second() == VertexFormat::Vector3usNormalized ||
               accessor->second() == VertexFormat::Vector4usNormalized)
                name = MeshAttribute::Color;
        /* Not a builtin MeshAttribute yet, but expected to be used by people
           until builtin support is added */
        } else if(baseAttributeName == "JOINTS"_s) {
            if(accessor->second() == VertexFormat::Vector4ub ||
               accessor->second() == VertexFormat::Vector4us)
                /** @todo update once these are builtin, but provide an opt-out
                    compatibility alias */
                name = _d->meshAttributesForName.at(baseAttributeName);
        } else if(baseAttributeName == "WEIGHTS"_s) {
            if(accessor->second() == VertexFormat::Vector4 ||
               accessor->second() == VertexFormat::Vector4ubNormalized ||
               accessor->second() == VertexFormat::Vector4usNormalized)
                /** @todo update once these are builtin, but provide an opt-out
                    compatibility alias */
                name = _d->meshAttributesForName.at(baseAttributeName);

        /* Object ID, name custom. To avoid confusion, print the error together
           with saying it's an object ID attribute */
        } else if(attribute.first() == configuration().value<Containers::StringView>("objectIdAttribute")) {
            name = MeshAttribute::ObjectId;

            if(accessor->second() != VertexFormat::UnsignedInt &&
               accessor->second() != VertexFormat::UnsignedShort &&
               accessor->second() != VertexFormat::UnsignedByte) {
                /* Here the VertexFormat prefix would not be confusing but
                   print it without to be consistent with other messages */
                Error{} << "Trade::GltfImporter::mesh(): unsupported object ID attribute" << attribute.first() << "type" << Debug::packed << accessor->second();
                return {};
            }

        /* Custom or unrecognized attributes, map to an ID. Any type is
           allowed. */
        } else {
            name = _d->meshAttributesForName.at(attribute.first());
        }

        if(name == MeshAttribute{}) {
            /* Here the VertexFormat prefix would not be confusing but print it
               without to be consistent with other messages */
            Error{} << "Trade::GltfImporter::mesh(): unsupported" << attribute.first() << "format" << Debug::packed << accessor->second();
            return {};
        }

        /* Remember which buffer the attribute is in and the range, for
           consecutive attribs expand the range */
        const Containers::Triple<Containers::ArrayView<const char>, UnsignedInt, UnsignedInt> bufferView = *_d->bufferViews[accessor->third()];
        if(attributeId == 0) {
            bufferId = bufferView.third();
            bufferRange = Math::Range1D<std::size_t>::fromSize(reinterpret_cast<std::size_t>(bufferView.first().data()), bufferView.first().size());
            vertexCount = accessor->first().size()[0];
        } else {
            /* ... and probably never will be */
            if(bufferView.third() != bufferId) {
                Error{} << "Trade::GltfImporter::mesh(): meshes spanning multiple buffers are not supported";
                return {};
            }

            bufferRange = Math::join(bufferRange, Math::Range1D<std::size_t>::fromSize(reinterpret_cast<std::size_t>(bufferView.first().data()), bufferView.first().size()));

            if(accessor->first().size()[0] != vertexCount) {
                Error{} << "Trade::GltfImporter::mesh(): mismatched vertex count for attribute" << attribute.first() << Debug::nospace << ", expected" << vertexCount << "but got" << accessor->first().size()[0];
                return {};
            }
        }

        /** @todo Check that accessor stride >= vertexFormatSize(format)? */

        /* Fill in an attribute. Points to the input data, will be patched to
           the output data once we know where it's allocated. */
        attributeData[attributeId++] = MeshAttributeData{name, accessor->second(), accessor->first()};
    }

    /* Verify we really filled all attributes */
    CORRADE_INTERNAL_ASSERT(attributeId == attributeData.size());

    /* Allocate & copy vertex data, if any */
    Containers::ArrayView<const char> inputVertexData{reinterpret_cast<const char*>(bufferRange.min()), bufferRange.size()};
    Containers::Array<char> vertexData{NoInit, bufferRange.size()};
    Utility::copy(inputVertexData, vertexData);

    /* Convert the attributes from relative to absolute, copy them to a
       non-growable array and do additional patching */
    for(std::size_t i = 0; i != attributeData.size(); ++i) {
        /* glTF only requires buffer views to be large enough to fit the actual
           data, not to have the size large enough to fit `count*stride`
           elements. The StridedArrayView expects the latter, so we fake the
           vertexData size to satisfy the assert. For simplicity we overextend
           by the whole stride instead of `offset + typeSize`, relying on
           parseAccessor() having checked the bounds already (and there is a
           similar workaround when populating the output view). */
        /** @todo instead of faking the size, split the offset into offset in
            whole strides and the remainder (Math::div), then form the view
            with offset in whole strides and then "shift" the view by the
            remainder (once there's StridedArrayView::shift() or some such) */
        Containers::StridedArrayView1D<char> data{{vertexData, vertexData.size() + attributeData[i].stride()},
            vertexData + attributeData[i].offset(inputVertexData),
            vertexCount, attributeData[i].stride()};

        attributeData[i] = MeshAttributeData{attributeData[i].name(),
            attributeData[i].format(), data};

        /* Flip Y axis of texture coordinates, unless it's done in the material
           instead */
        if(attributeData[i].name() == MeshAttribute::TextureCoordinates && !_d->textureCoordinateYFlipInMaterial) {
           if(attributeData[i].format() == VertexFormat::Vector2)
                for(auto& c: Containers::arrayCast<Vector2>(data))
                    c.y() = 1.0f - c.y();
            else if(attributeData[i].format() == VertexFormat::Vector2ubNormalized)
                for(auto& c: Containers::arrayCast<Vector2ub>(data))
                    c.y() = 255 - c.y();
            else if(attributeData[i].format() == VertexFormat::Vector2usNormalized)
                for(auto& c: Containers::arrayCast<Vector2us>(data))
                    c.y() = 65535 - c.y();
            /* For these it's always done in the material texture transform as
               we can't do a 1 - y flip like above. These are allowed only by
               the KHR_mesh_quantization formats and in that case the texture
               transform should be always present. */
            /* LCOV_EXCL_START */
            else if(attributeData[i].format() != VertexFormat::Vector2bNormalized &&
                    attributeData[i].format() != VertexFormat::Vector2sNormalized &&
                    attributeData[i].format() != VertexFormat::Vector2ub &&
                    attributeData[i].format() != VertexFormat::Vector2b &&
                    attributeData[i].format() != VertexFormat::Vector2us &&
                    attributeData[i].format() != VertexFormat::Vector2s)
                CORRADE_INTERNAL_ASSERT_UNREACHABLE();
            /* LCOV_EXCL_STOP */
        }
    }

    /* Indices */
    MeshIndexData indices;
    Containers::Array<char> indexData;
    if(const Utility::JsonToken* gltfIndices = gltfPrimitive.find("indices"_s)) {
        if(!_d->gltf->parseUnsignedInt(*gltfIndices)) {
            Error{} << "Trade::GltfImporter::mesh(): invalid indices property";
            return {};
        }
        /* Bounds check is done in parseAccessor() below, no need to do it
           here again */

        Containers::Optional<Containers::Triple<Containers::StridedArrayView2D<const char>, VertexFormat, UnsignedInt>> accessor = parseAccessor("Trade::GltfImporter::mesh():", gltfIndices->asUnsignedInt());
        if(!accessor) return {};

        MeshIndexType type;
        if(accessor->second() == VertexFormat::UnsignedByte)
            type = MeshIndexType::UnsignedByte;
        else if(accessor->second() == VertexFormat::UnsignedShort)
            type = MeshIndexType::UnsignedShort;
        else if(accessor->second() == VertexFormat::UnsignedInt)
            type = MeshIndexType::UnsignedInt;
        else {
            /* Since we're abusing VertexFormat for all formats, print just the
               enum value without the prefix to avoid cofusion */
            Error{} << "Trade::GltfImporter::mesh(): unsupported index type" << Debug::packed << accessor->second();
            return {};
        }

        if(!accessor->first().isContiguous()) {
            Error{} << "Trade::GltfImporter::mesh(): index buffer view is not contiguous";
            return {};
        }

        Containers::ArrayView<const char> srcContiguous = accessor->first().asContiguous();
        indexData = Containers::Array<char>{srcContiguous.size()};
        Utility::copy(srcContiguous, indexData);
        indices = MeshIndexData{type, indexData};
    }

    /* If we have an index-less attribute-less mesh, glTF has no way to supply
       a vertex count, so return 0 */
    if(!indices.data().size() && !attributeData.size())
        return MeshData{primitive, 0};

    return MeshData{primitive,
        std::move(indexData), indices,
        std::move(vertexData), std::move(attributeData),
        vertexCount, &gltfPrimitive};
}

MeshAttribute GltfImporter::doMeshAttributeForName(const Containers::StringView name) {
    return _d ? _d->meshAttributesForName[name] : MeshAttribute{};
}

Containers::String GltfImporter::doMeshAttributeName(const UnsignedShort name) {
    return _d && name < _d->meshAttributeNames.size() ?
        _d->meshAttributeNames[name] : "";
}

UnsignedInt GltfImporter::doMaterialCount() const {
    return _d->gltfMaterials.size();
}

Int GltfImporter::doMaterialForName(const Containers::StringView name) {
    if(!_d->materialsForName) {
        _d->materialsForName.emplace();
        _d->materialsForName->reserve(_d->gltfMaterials.size());
        for(std::size_t i = 0; i != _d->gltfMaterials.size(); ++i)
            if(const Containers::StringView n = _d->gltfMaterials[i].second())
                _d->materialsForName->emplace(n, i);
    }

    const auto found = _d->materialsForName->find(name);
    return found == _d->materialsForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doMaterialName(const UnsignedInt id) {
    return _d->gltfMaterials[id].second();
}

namespace {

/** @todo turn this into a helper API on MaterialAttributeData and then drop
    from here and AssimpImporter */
bool checkMaterialAttributeSize(const Containers::StringView name, const MaterialAttributeType type, const void* const value = nullptr) {
    std::size_t valueSize;
    if(type == MaterialAttributeType::String) {
        CORRADE_INTERNAL_ASSERT(value);
        /* +2 are null byte and size */
        valueSize = static_cast<const Containers::StringView*>(value)->size() + 2;
    } else
        valueSize = materialAttributeTypeSize(type);

    /* +1 is the key null byte */
    if(valueSize + name.size() + 1 + sizeof(MaterialAttributeType) > sizeof(MaterialAttributeData)) {
        Warning{} << "Trade::GltfImporter::material(): property" << name <<
            "is too large with" << valueSize + name.size() << "bytes, skipping";
        return false;
    }

    return true;
}

Containers::Optional<MaterialAttributeData> parseMaterialAttribute(Utility::Json& gltf, const Utility::JsonToken& gltfKey) {
    /* Not const, gets modified if the first letter isn't lowercase */
    Containers::StringView name = gltfKey.asString();
    if(!name) {
        Warning{} << "Trade::GltfImporter::material(): property with an empty name, skipping";
        return {};
    }

    CORRADE_INTERNAL_ASSERT(gltfKey.firstChild());
    const Utility::JsonToken& gltfValue = *gltfKey.firstChild();

    /* We only need temporary storage for parsing primitive (arrays) as bool/
       Float/Vector[2/3/4]. Other types/sizes are either converted or ignored,
       so we know the upper limit on the data size. The alignas prevents
       unaligned reads for individual floats. For strings,
       MaterialAttributeData expects a pointer to StringView. */
    alignas(4) char attributeData[16];
    Containers::StringView attributeStringView;
    MaterialAttributeType type{};

    /* Generic object, skip. Not parsing textureInfo objects here because
       they're only needed by extensions but not by extras. They may also
       append more than one attribute, so this is handled directly in the
       extension parsing loop. */
    if(gltfValue.type() == Utility::JsonToken::Type::Object) {
        Warning{} << "Trade::GltfImporter::material(): property" << name << "is an object, skipping";
        return {};

    /* Array, hopefully numeric */
    } else if(gltfValue.type() == Utility::JsonToken::Type::Array) {
        for(const Utility::JsonToken& i: *gltf.parseArray(gltfValue)) {
            if(i.type() != Utility::JsonToken::Type::Number) {
                Warning{} << "Trade::GltfImporter::material(): property" << name << "is not a numeric array, skipping";
                return {};
            }
        }

        /* Always interpret numbers as floats because the type can be
           ambiguous. E.g. integer attributes may use exponent notation and
           decimal points, making correct type detection depend on glTF
           exporter behaviour. */
        Containers::Optional<Containers::StridedArrayView1D<const float>> valueArray = gltf.parseFloatArray(gltfValue);
        /* No use importing arbitrarily-sized arrays of primitives, those are
           currently not used in any glTF extension */
        if(!valueArray || valueArray->size() < 1 || valueArray->size() > 4) {
            Warning{} << "Trade::GltfImporter::material(): property" << name << "is an invalid or unrepresentable numeric vector, skipping";
            return {};
        }

        constexpr MaterialAttributeType vectorType[]{
            MaterialAttributeType::Float,
            MaterialAttributeType::Vector2,
            MaterialAttributeType::Vector3,
            MaterialAttributeType::Vector4
        };
        type = vectorType[valueArray->size() - 1];
        Utility::copy(*valueArray, {reinterpret_cast<Float*>(attributeData), valueArray->size()});

    /* Null. Not sure what for, skipping. If the token is not actually a valid
       null value, the error gets silently ignored. */
    } else if(gltfValue.type() == Utility::JsonToken::Type::Null) {
        Warning{} << "Trade::GltfImporter::material(): property" << name << "is a null, skipping";
        return {};

    /* Bool */
    } else if(gltfValue.type() == Utility::JsonToken::Type::Bool) {
        if(const Containers::Optional<bool> b = gltf.parseBool(gltfValue)) {
            type = MaterialAttributeType::Bool;
            *reinterpret_cast<bool*>(attributeData) = *b;
        } else {
            Warning{} << "Trade::GltfImporter::material(): property" << name << "is invalid, skipping";
            return {};
        }

    /* Number */
    } else if(gltfValue.type() == Utility::JsonToken::Type::Number) {
        /* Always interpret numbers as floats because the type can be
           ambiguous. E.g. integer attributes may use exponent notation and
           decimal points, making correct type detection depend on glTF
           exporter behaviour. */
        if(const Containers::Optional<Float> f = gltf.parseFloat(gltfValue)) {
            type = MaterialAttributeType::Float;
            *reinterpret_cast<Float*>(attributeData) = *f;
        } else {
            Warning{} << "Trade::GltfImporter::material(): property" << name << "is invalid, skipping";
            return {};
        }

    /* String */
    } else if(gltfValue.type() == Utility::JsonToken::Type::String) {
        if(const Containers::Optional<Containers::StringView> s = gltf.parseString(gltfValue)) {
            type = MaterialAttributeType::String;
            attributeStringView = *s;
        } else {
            Warning{} << "Trade::GltfImporter::material(): property" << name << "is invalid, skipping";
            return {};
        }

    /* No other token types exist */
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    CORRADE_INTERNAL_ASSERT(type != MaterialAttributeType{});

    const void* const valuePointer = type == MaterialAttributeType::String ?
        static_cast<const void*>(&attributeStringView) : static_cast<const void*>(attributeData);
    if(!checkMaterialAttributeSize(name, type, valuePointer))
        return {};

    /* Uppercase attribute names are reserved. Standard glTF (extension)
       attributes should all be lowercase but we don't have this guarantee for
       extras attributes. Can't use String::nullTerminatedView() here because
       JSON tokens are not null-terminated. */
    Containers::String nameLowercase;
    if(std::isupper(static_cast<unsigned char>(name.front()))) {
        nameLowercase = name;
        nameLowercase.front() = std::tolower(static_cast<unsigned char>(name.front()));
        name = nameLowercase;
    }

    return MaterialAttributeData{name, type, valuePointer};
}

}

bool GltfImporter::materialTexture(const Utility::JsonToken& gltfTexture, Containers::Array<MaterialAttributeData>& attributes, const Containers::StringView attribute, const Containers::StringView matrixAttribute, const Containers::StringView coordinateAttribute) {
    if(!_d->gltf->parseObject(gltfTexture)) {
        Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "property";
        return false;
    }

    const Utility::JsonToken* const gltfIndex = gltfTexture.find("index"_s);
    if(!gltfIndex || !_d->gltf->parseUnsignedInt(*gltfIndex)) {
        Error{} << "Trade::GltfImporter::material(): missing or invalid" << gltfTexture.parent()->asString() << "index property";
        return false;
    }
    if(gltfIndex->asUnsignedInt() >= _d->gltfTextures.size()) {
        Error{} << "Trade::GltfImporter::material():" << gltfTexture.parent()->asString() << "index" << gltfIndex->asUnsignedInt() << "out of range for" << _d->gltfTextures.size() << "textures";
        return false;
    }

    /* Texture coordinate is optional, defaulting to 0 */
    UnsignedInt texCoord = 0;
    if(const Utility::JsonToken* const gltfTexCoord = gltfTexture.find("texCoord"_s)) {
        if(!_d->gltf->parseUnsignedInt(*gltfTexCoord)) {
            Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "texcoord property";
            return false;
        }

        texCoord = gltfTexCoord->asUnsignedInt();
    }

    /* Extensions */
    const Utility::JsonToken* gltfKhrTextureTransform = nullptr;
    if(const Utility::JsonToken* const gltfExtensions = gltfTexture.find("extensions"_s)) {
        if(!_d->gltf->parseObject(*gltfExtensions)) {
            Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "extensions property";
            return false;
        }

        /* Texture transform. Because texture coordinates were Y-flipped, we
           first unflip them back, apply the transform (which assumes origin at
           bottom left and Y down) and then flip the result again. Sanity of
           the following verified with https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/TextureTransformTest */
        gltfKhrTextureTransform = gltfExtensions->find("KHR_texture_transform"_s);
        if(gltfKhrTextureTransform && !_d->gltf->parseObject(*gltfKhrTextureTransform)) {
            Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "KHR_texture_transform extension";
            return false;
        }
        if(gltfKhrTextureTransform && checkMaterialAttributeSize(matrixAttribute, MaterialAttributeType::Matrix3x3)) {
            Matrix3 matrix;

            /* If material needs an Y-flip, the mesh doesn't have the texture
               coordinates flipped and thus we don't need to unflip them
               first */
            if(!_d->textureCoordinateYFlipInMaterial)
                matrix = Matrix3::translation(Vector2::yAxis(1.0f))*
                         Matrix3::scaling(Vector2::yScale(-1.0f));

            /* The extension can override texture coordinate index (for example
               to have the unextended coordinates already transformed, and
               applying transformation to a different set) */
            if(const Utility::JsonToken* const gltfTexCoord = gltfKhrTextureTransform->find("texCoord"_s)) {
                if(!_d->gltf->parseUnsignedInt(*gltfTexCoord)) {
                    Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "KHR_texture_transform texcoord property";
                    return false;
                }

                texCoord = gltfTexCoord->asUnsignedInt();
            }

            Vector2 scaling{1.0f};
            if(const Utility::JsonToken* const gltfScale = gltfKhrTextureTransform->find("scale"_s)) {
                const Containers::Optional<Containers::StridedArrayView1D<const float>> scalingArray = _d->gltf->parseFloatArray(*gltfScale, 2);
                if(!scalingArray) {
                    Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "KHR_texture_transform scale property";
                    return false;
                }

                Utility::copy(*scalingArray, scaling.data());
            }
            matrix = Matrix3::scaling(scaling)*matrix;

            Rad rotation;
            if(const Utility::JsonToken* const gltfRotation = gltfKhrTextureTransform->find("rotation"_s)) {
                if(!_d->gltf->parseFloat(*gltfRotation)) {
                    Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "KHR_texture_transform rotation property";
                    return false;
                }

                rotation = Rad(gltfRotation->asFloat());
            }
            /* Because we import images with Y flipped, counterclockwise
               rotation is now clockwise. This has to be done in addition to
               the Y flip/unflip. */
            matrix = Matrix3::rotation(-rotation)*matrix;

            Vector2 offset;
            if(const Utility::JsonToken* const gltfOffset = gltfKhrTextureTransform->find("offset"_s)) {
                const Containers::Optional<Containers::StridedArrayView1D<const float>> offsetArray = _d->gltf->parseFloatArray(*gltfOffset, 2);
                if(!offsetArray) {
                    Error{} << "Trade::GltfImporter::material(): invalid" << gltfTexture.parent()->asString() << "KHR_texture_transform offset property";
                    return false;
                }

                Utility::copy(*offsetArray, offset.data());
            }
            matrix = Matrix3::translation(offset)*matrix;

            matrix = Matrix3::translation(Vector2::yAxis(1.0f))*
                     Matrix3::scaling(Vector2::yScale(-1.0f))*matrix;

            arrayAppend(attributes, InPlaceInit, matrixAttribute, matrix);
        }
    }

    /* In case the material had no texture transformation but still needs an
       Y-flip, put it there */
    if(!gltfKhrTextureTransform && _d->textureCoordinateYFlipInMaterial &&
       checkMaterialAttributeSize(matrixAttribute, MaterialAttributeType::Matrix3x3))
    {
        arrayAppend(attributes, InPlaceInit, matrixAttribute,
            Matrix3::translation(Vector2::yAxis(1.0f))*
            Matrix3::scaling(Vector2::yScale(-1.0f)));
    }

    /* Add texture coordinate set if non-zero. The KHR_texture_transform could
       be modifying it, so do that after */
    if(texCoord != 0 && checkMaterialAttributeSize(coordinateAttribute, MaterialAttributeType::UnsignedInt))
        arrayAppend(attributes, InPlaceInit, coordinateAttribute, texCoord);

    /* In some cases (when dealing with packed textures), we're parsing &
       adding texture coordinates and matrix multiple times, but adding the
       packed texture ID just once. In other cases the attribute is invalid. */
    if(!attribute.isEmpty() && checkMaterialAttributeSize(attribute, MaterialAttributeType::UnsignedInt)) {
        arrayAppend(attributes, InPlaceInit, attribute, gltfIndex->asUnsignedInt());
    }

    return true;
}

Containers::Optional<MaterialData> GltfImporter::doMaterial(const UnsignedInt id) {
    const Utility::JsonToken& gltfMaterial = _d->gltfMaterials[id].first();

    Containers::Array<UnsignedInt> layers;
    Containers::Array<MaterialAttributeData> attributes;
    MaterialTypes types;

    /* Alpha mode and mask. Opaque is default in both Magnum's MaterialData and
       glTF, no need to add anything if not present. */
    if(const Utility::JsonToken* const gltfAlphaMode = gltfMaterial.find("alphaMode"_s)) {
        if(!_d->gltf->parseString(*gltfAlphaMode)) {
            Error{} << "Trade::GltfImporter::material(): invalid alphaMode property";
            return {};
        }

        const Containers::StringView mode = gltfAlphaMode->asString();
        if(mode == "BLEND"_s) {
            arrayAppend(attributes, InPlaceInit, MaterialAttribute::AlphaBlend, true);

        } else if(mode == "MASK"_s) {
            /* Cutoff is optional, defaults to 0.5 */
            Float mask = 0.5f;
            if(const Utility::JsonToken* const gltfAlphaCutoff = gltfMaterial.find("alphaCutoff"_s)) {
                if(!_d->gltf->parseFloat(*gltfAlphaCutoff)) {
                    Error{} << "Trade::GltfImporter::material(): invalid alphaCutoff property";
                    return {};
                }

                mask = gltfAlphaCutoff->asFloat();
            }
            arrayAppend(attributes, InPlaceInit, MaterialAttribute::AlphaMask, mask);

        } else if(mode == "OPAQUE"_s) {
            /* If the attribute was explicitly set to a default in the file,
               add it also explicitly here for consistency */
            arrayAppend(attributes, InPlaceInit, MaterialAttribute::AlphaBlend, false);

        } else {
            Error{} << "Trade::GltfImporter::material(): unrecognized alphaMode" << mode;
            return {};
        }
    }

    /* Double sided. False is default in both Magnum's MaterialData and glTF,
       no need to add anything if not present. */
    if(const Utility::JsonToken* const gltfDoubleSided = gltfMaterial.find("doubleSided"_s)) {
        if(!_d->gltf->parseBool(*gltfDoubleSided)) {
            Error{} << "Trade::GltfImporter::material(): invalid doubleSided property";
            return {};
        }

        arrayAppend(attributes, InPlaceInit, MaterialAttribute::DoubleSided, gltfDoubleSided->asBool());
    }

    /* Core metallic/roughness material */
    if(const Utility::JsonToken* const gltfPbrMetallicRoughness = gltfMaterial.find("pbrMetallicRoughness"_s)) {
        if(!_d->gltf->parseObject(*gltfPbrMetallicRoughness)) {
            Error{} << "Trade::GltfImporter::material(): invalid pbrMetallicRoughness property";
            return {};
        }

        types |= MaterialType::PbrMetallicRoughness;

        /* Base color factor. Vector of 1.0 is default in both Magnum's
           MaterialData and glTF, no need to add anything if not present. */
        if(const Utility::JsonToken* const gltfBaseColorFactor = gltfPbrMetallicRoughness->find("baseColorFactor"_s)) {
            const Containers::Optional<Containers::StridedArrayView1D<const float>> baseColorArray = _d->gltf->parseFloatArray(*gltfBaseColorFactor, 4);
            if(!baseColorArray) {
                Error{} << "Trade::GltfImporter::material(): invalid pbrMetallicRoughness baseColorFactor property";
                return {};
            }

            Color4 baseColor{NoInit};
            Utility::copy(*baseColorArray, baseColor.data());
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::BaseColor, baseColor);
        }

        /* Metallic factor. 1.0 is default in both Magnum's MaterialData and
           glTF, no need to add anything if not present. */
        if(const Utility::JsonToken* const gltfMetallicFactor = gltfPbrMetallicRoughness->find("metallicFactor"_s)) {
            if(!_d->gltf->parseFloat(*gltfMetallicFactor)) {
                Error{} << "Trade::GltfImporter::material(): invalid pbrMetallicRoughness metallicFactor property";
                return {};
            }

            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Metalness, gltfMetallicFactor->asFloat());
        }

        /* Roughness factor. 1.0 is default in both Magnum's MaterialData and
           glTF, no need to add anything if not present. */
        if(const Utility::JsonToken* const gltfRoughnessFactor = gltfPbrMetallicRoughness->find("roughnessFactor"_s)) {
            if(!_d->gltf->parseFloat(*gltfRoughnessFactor)) {
                Error{} << "Trade::GltfImporter::material(): invalid pbrMetallicRoughness roughnessFactor property";
                return {};
            }

            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Roughness, gltfRoughnessFactor->asFloat());
        }

        if(const Utility::JsonToken* const gltfBaseColorTexture = gltfPbrMetallicRoughness->find("baseColorTexture"_s)) {
            if(!materialTexture(*gltfBaseColorTexture, attributes,
                "BaseColorTexture"_s,
                "BaseColorTextureMatrix"_s,
                "BaseColorTextureCoordinates"_s)) return {};
        }

        if(const Utility::JsonToken* const gltfMetallicRoughnessTexture = gltfPbrMetallicRoughness->find("metallicRoughnessTexture"_s)) {
            if(!materialTexture(*gltfMetallicRoughnessTexture, attributes,
                "NoneRoughnessMetallicTexture"_s,
                "MetalnessTextureMatrix"_s,
                "MetalnessTextureCoordinates"_s)) return {};

            /* Add the matrix/coordinates attributes also for the roughness
               texture, but skip adding the texture ID again. If the above
               didn't fail, this shouldn't either. */
            CORRADE_INTERNAL_ASSERT_OUTPUT(materialTexture(
                *gltfMetallicRoughnessTexture, attributes,
                {},
                "RoughnessTextureMatrix"_s,
                "RoughnessTextureCoordinates"_s));
        }

        /** @todo Support for KHR_materials_specular? This adds an explicit
            F0 (texture) and a scalar factor (texture) for the entire specular
            reflection to a metallic/roughness material. Currently imported as
            a custom layer below. */
    }

    /* Extensions. Go through the object, filter out what's supported directly
       and put the rest into a list to be processed later. */
    const Utility::JsonToken* gltfPbrSpecularGlossiness = nullptr;
    const Utility::JsonToken* gltfUnlit = nullptr;
    const Utility::JsonToken* gltfClearcoat = nullptr;
    Containers::Array<Containers::Reference<const Utility::JsonToken>> gltfExtensionsKeys;
    if(const Utility::JsonToken* const gltfExtensions = gltfMaterial.find("extensions"_s)) {
        if(!_d->gltf->parseObject(*gltfExtensions)) {
            Error{} << "Trade::GltfImporter::material(): invalid extensions property";
            return {};
        }

        for(Utility::JsonObjectItem gltfExtension: gltfExtensions->asObject()) {
            const Containers::StringView extensionName = gltfExtension.key();
            if(!_d->gltf->parseObject(gltfExtension.value())) {
                Error{} << "Trade::GltfImporter::material(): invalid" << extensionName << "extension property";
                return {};
            }

            if(extensionName == "KHR_materials_pbrSpecularGlossiness"_s)
                gltfPbrSpecularGlossiness = &gltfExtension.value();
            else if(extensionName == "KHR_materials_unlit"_s)
                gltfUnlit = &gltfExtension.value();
            else if(extensionName == "KHR_materials_clearcoat"_s)
                gltfClearcoat = &gltfExtension.value();
            else
                arrayAppend(gltfExtensionsKeys, InPlaceInit, gltfExtension);
        }
    }

    /* Specular/glossiness material */
    if(gltfPbrSpecularGlossiness) {
        types |= MaterialType::PbrSpecularGlossiness;

        /* Token checked for object type above already */

        /* Diffuse factor. Vector of 1.0 is default in both Magnum's
           MaterialData and glTF, no need to add anything if not present. */
        if(const Utility::JsonToken* const gltfDiffuseFactor = gltfPbrSpecularGlossiness->find("diffuseFactor"_s)) {
            const Containers::Optional<Containers::StridedArrayView1D<const float>> diffuseFactorArray = _d->gltf->parseFloatArray(*gltfDiffuseFactor, 4);
            if(!diffuseFactorArray) {
                Error{} << "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness diffuseFactor property";
                return {};
            }

            Color4 diffuseFactor{NoInit};
            Utility::copy(*diffuseFactorArray, diffuseFactor.data());
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::DiffuseColor, diffuseFactor);
        }

        /* Specular factor. Vector of 1.0 is default in both Magnum's
           MaterialData and glTF, no need to add anything if not present. */
        if(const Utility::JsonToken* const gltfSpecularFactor = gltfPbrSpecularGlossiness->find("specularFactor"_s)) {
            const Containers::Optional<Containers::StridedArrayView1D<const float>> specularFactorArray = _d->gltf->parseFloatArray(*gltfSpecularFactor, 3);
            if(!specularFactorArray) {
                Error{} << "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness specularFactor property";
                return {};
            }

            /* Specular is 3-component in glTF, alpha should be 0 to not
               affect transparent materials */
            Color4 specularFactor{NoInit};
            specularFactor.a() = 0.0f;
            Utility::copy(*specularFactorArray, Containers::arrayView(specularFactor.data()).prefix(3));
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::SpecularColor, specularFactor);
        }

        /* Glossiness factor. 1.0 is default in both Magnum's MaterialData and
           glTF, no need to add anything if not present. */
        if(const Utility::JsonToken* const gltfGlossinessFactor = gltfPbrSpecularGlossiness->find("glossinessFactor"_s)) {
            if(!_d->gltf->parseFloat(*gltfGlossinessFactor)) {
                Error{} << "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness glossinessFactor property";
                return {};
            }

            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Glossiness, gltfGlossinessFactor->asFloat());
        }

        if(const Utility::JsonToken* const gltfBaseColorTexture = gltfPbrSpecularGlossiness->find("diffuseTexture"_s)) {
            if(!materialTexture(*gltfBaseColorTexture, attributes,
                "DiffuseTexture"_s,
                "DiffuseTextureMatrix"_s,
                "DiffuseTextureCoordinates"_s)) return {};
        }

        if(const Utility::JsonToken* const gltfSpecularGlossinessTexture = gltfPbrSpecularGlossiness->find("specularGlossinessTexture"_s)) {
           if(!materialTexture(*gltfSpecularGlossinessTexture, attributes,
                "SpecularGlossinessTexture"_s,
                "SpecularTextureMatrix"_s,
                "SpecularTextureCoordinates"_s)) return {};

            /* Add the matrix/coordinates attributes also for the glossiness
               texture, but skip adding the texture ID again. If the above
               didn't fail, this shouldn't either. */
            CORRADE_INTERNAL_ASSERT_OUTPUT(materialTexture(
                *gltfSpecularGlossinessTexture, attributes,
                {},
                "GlossinessTextureMatrix"_s,
                "GlossinessTextureCoordinates"_s));
        }
    }

    /* Unlit material -- reset all types and add just Flat */
    if(gltfUnlit) {
        types = MaterialType::Flat;

        /* Token checked for object type above already */
    }

    /* Normal texture */
    if(const Utility::JsonToken* const gltfNormalTexture = gltfMaterial.find("normalTexture"_s)) {
        if(!materialTexture(*gltfNormalTexture, attributes,
            "NormalTexture"_s,
            "NormalTextureMatrix"_s,
            "NormalTextureCoordinates"_s)) return {};

        /* Scale. 1.0 is default in both Magnum's MaterialData and glTF, no
           need to add anything if not present. */
        if(const Utility::JsonToken* const gltfNormalTextureScale = gltfNormalTexture->find("scale"_s)) {
            if(!_d->gltf->parseFloat(*gltfNormalTextureScale)) {
                Error{} << "Trade::GltfImporter::material(): invalid normalTexture scale property";
                return {};
            }

            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::NormalTextureScale,
                gltfNormalTextureScale->asFloat());
        }
    }

    /* Occlusion texture */
    if(const Utility::JsonToken* const gltfOcclusionTexture = gltfMaterial.find("occlusionTexture"_s)) {
        if(!materialTexture(*gltfOcclusionTexture, attributes,
            "OcclusionTexture"_s,
            "OcclusionTextureMatrix"_s,
            "OcclusionTextureCoordinates"_s)) return {};

        /* Strength. 1.0 is default in both Magnum's MaterialData and glTF, no
           need to add anything if not present. */
        if(const Utility::JsonToken* const gltfOcclusionTextureStrength = gltfOcclusionTexture->find("strength"_s)) {
            if(!_d->gltf->parseFloat(*gltfOcclusionTextureStrength)) {
                Error{} << "Trade::GltfImporter::material(): invalid occlusionTexture strength property";
                return {};
            }

            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::OcclusionTextureStrength,
                gltfOcclusionTextureStrength->asFloat());
        }
    }

    /* Emissive factor. Vector of 1.0 is default in both Magnum's MaterialData
       and glTF, no need to add anything if not present. */
    if(const Utility::JsonToken* const gltfEmissiveFactor = gltfMaterial.find("emissiveFactor"_s)) {
        const Containers::Optional<Containers::StridedArrayView1D<const float>> emissiveFactorArray = _d->gltf->parseFloatArray(*gltfEmissiveFactor, 3);
        if(!emissiveFactorArray) {
            Error{} << "Trade::GltfImporter::material(): invalid emissiveFactor property";
            return {};
        }

        Color3 emissiveColor{NoInit};
        Utility::copy(*emissiveFactorArray, emissiveColor.data());
        arrayAppend(attributes, InPlaceInit,
            MaterialAttribute::EmissiveColor, emissiveColor);
    }

    /* Emissive texture */
    if(const Utility::JsonToken* const gltfEmissiveTexture = gltfMaterial.find("emissiveTexture"_s)) {
        if(!materialTexture(*gltfEmissiveTexture, attributes,
            "EmissiveTexture"_s,
            "EmissiveTextureMatrix"_s,
            "EmissiveTextureCoordinates"_s)) return {};
    }

    /* Phong material fallback for backwards compatibility */
    if(configuration().value<bool>("phongMaterialFallback")) {
        /* This adds a Phong type even to Flat materials because that's exactly
           how it behaved before */
        types |= MaterialType::Phong;

        /* Create Diffuse attributes from BaseColor */
        Containers::Optional<Color4> diffuseColor;
        Containers::Optional<UnsignedInt> diffuseTexture;
        Containers::Optional<Matrix3> diffuseTextureMatrix;
        Containers::Optional<UnsignedInt> diffuseTextureCoordinates;
        for(const MaterialAttributeData& attribute: attributes) {
            if(attribute.name() == "BaseColor"_s)
                diffuseColor = attribute.value<Color4>();
            else if(attribute.name() == "BaseColorTexture"_s)
                diffuseTexture = attribute.value<UnsignedInt>();
            else if(attribute.name() == "BaseColorTextureMatrix"_s)
                diffuseTextureMatrix = attribute.value<Matrix3>();
            else if(attribute.name() == "BaseColorTextureCoordinates"_s)
                diffuseTextureCoordinates = attribute.value<UnsignedInt>();
        }

        /* But if there already are those from the specular/glossiness
           material, don't add them again. Has to be done in a separate pass
           to avoid resetting too early. */
        for(const MaterialAttributeData& attribute: attributes) {
            if(attribute.name() == "DiffuseColor"_s)
                diffuseColor = Containers::NullOpt;
            else if(attribute.name() == "DiffuseTexture"_s)
                diffuseTexture = Containers::NullOpt;
            else if(attribute.name() == "DiffuseTextureMatrix"_s)
                diffuseTextureMatrix = Containers::NullOpt;
            else if(attribute.name() == "DiffuseTextureCoordinates"_s)
                diffuseTextureCoordinates = Containers::NullOpt;
        }

        if(diffuseColor)
            arrayAppend(attributes, InPlaceInit, MaterialAttribute::DiffuseColor, *diffuseColor);
        if(diffuseTexture)
            arrayAppend(attributes, InPlaceInit, MaterialAttribute::DiffuseTexture, *diffuseTexture);
        if(diffuseTextureMatrix)
            arrayAppend(attributes, InPlaceInit, MaterialAttribute::DiffuseTextureMatrix, *diffuseTextureMatrix);
        if(diffuseTextureCoordinates)
            arrayAppend(attributes, InPlaceInit, MaterialAttribute::DiffuseTextureCoordinates, *diffuseTextureCoordinates);
    }

    /* Extras -- application-specific data, added to the base layer */
    if(const Utility::JsonToken* const gltfExtras = gltfMaterial.find("extras"_s)) {
        /* Theoretically extras can be any token type but the glTF spec
           recommends objects for interoperability, makes our life easier, too:
           https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-extras */
        if(gltfExtras->type() == Utility::JsonToken::Type::Object) {
            if(_d->gltf->parseObject(*gltfExtras)) {
                Containers::Array<Containers::Reference<const Utility::JsonToken>> gltfExtraKeys;
                for(const Utility::JsonToken& i: gltfExtras->asObject())
                    arrayAppend(gltfExtraKeys, InPlaceInit, i);

                /* Sort and remove duplicates except the last one. We don't
                   need to cross-check for duplicates in the base layer because
                   those are all internal uppercase names and we make all names
                   lowercase. */
                const std::size_t uniqueCount = stableSortRemoveDuplicatesToPrefix(arrayView(gltfExtraKeys),
                    [](const Utility::JsonToken& a, const Utility::JsonToken& b) {
                        return a.asString() < b.asString();
                    },
                    [](const Utility::JsonToken& a, const Utility::JsonToken& b) {
                        return a.asString() == b.asString();
                    });

                arrayReserve(attributes, attributes.size() + uniqueCount);
                /** @todo use suffix() once it takes suffix size and not prefix size */
                for(const Utility::JsonToken& gltfKey: gltfExtraKeys.exceptPrefix(gltfExtraKeys.size() - uniqueCount)) {
                    if(const Containers::Optional<MaterialAttributeData> parsed = parseMaterialAttribute(*_d->gltf, gltfKey))
                        arrayAppend(attributes, *parsed);
                }

            } else Warning{} << "Trade::GltfImporter::material(): extras object has invalid keys, skipping";

        } else Warning{} << "Trade::GltfImporter::material(): extras property is not an object, skipping";
    }

    /* Clear coat layer -- needs to be after all base material attributes */
    if(gltfClearcoat) {
        types |= MaterialType::PbrClearCoat;

        /* Token checked for object type above already */

        /* Add a new layer -- this works both if layers are empty and if
           there's something already */
        arrayAppend(layers, UnsignedInt(attributes.size()));
        arrayAppend(attributes, InPlaceInit, MaterialLayer::ClearCoat);

        /* Weirdly enough, the KHR_materials_clearcoat has the factor
           defaulting to 0.0, which means if a texture is specified both have
           to be present to not have the texture canceled out. Magnum has 1.0
           as a default, so we add an explicit 0.0 if the factor is not
           present. */
        if(const Utility::JsonToken* const gltfClearcoatFactor = gltfClearcoat->find("clearcoatFactor"_s)) {
            if(!_d->gltf->parseFloat(*gltfClearcoatFactor)) {
                Error{} << "Trade::GltfImporter::material(): invalid KHR_materials_clearcoat clearcoatFactor property";
                return {};
            }

            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::LayerFactor, gltfClearcoatFactor->asFloat());
        } else arrayAppend(attributes, InPlaceInit,
            MaterialAttribute::LayerFactor, 0.0f);

        if(const Utility::JsonToken* const gltfClearcoatTexture = gltfClearcoat->find("clearcoatTexture"_s)) {
            if(!materialTexture(*gltfClearcoatTexture,
                attributes,
                "LayerFactorTexture"_s,
                "LayerFactorTextureMatrix"_s,
                "LayerFactorTextureCoordinates"_s)) return {};
        }

        /* Same as with gltfClearcoatFactor -- Magnum's default is 1.0 to not
           have to specify both if a texture is present, so add an explicit 0.0
           if the factor is not present. */
        if(const Utility::JsonToken* const gltfRoughnessFactor = gltfClearcoat->find("clearcoatRoughnessFactor"_s)) {
            if(!_d->gltf->parseFloat(*gltfRoughnessFactor)) {
                Error{} << "Trade::GltfImporter::material(): invalid KHR_materials_clearcoat roughnessFactor property";
                return {};
            }

            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Roughness, gltfRoughnessFactor->asFloat());
        } else arrayAppend(attributes, InPlaceInit,
            MaterialAttribute::Roughness, 0.0f);

        if(const Utility::JsonToken* const gltfRoughnessTexture = gltfClearcoat->find("clearcoatRoughnessTexture"_s)) {
            if(!materialTexture(*gltfRoughnessTexture, attributes,
                "RoughnessTexture"_s,
                "RoughnessTextureMatrix"_s,
                "RoughnessTextureCoordinates"_s)) return {};

            /* The extension description doesn't mention it, but the schema
               says the clearcoat roughness is actually in the G channel:
               https://github.com/KhronosGroup/glTF/blob/dc5519b9ce9834f07c30ec4c957234a0cd6280a2/extensions/2.0/Khronos/KHR_materials_clearcoat/schema/glTF.KHR_materials_clearcoat.schema.json#L32 */
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::RoughnessTextureSwizzle,
                MaterialTextureSwizzle::G);
        }

        if(const Utility::JsonToken* const gltfNormalTexture = gltfClearcoat->find("clearcoatNormalTexture"_s)) {
            if(!materialTexture(*gltfNormalTexture, attributes,
                "NormalTexture"_s,
                "NormalTextureMatrix"_s,
                "NormalTextureCoordinates"_s)) return {};

            /* Scale. 1.0 is default in both Magnum's MaterialData and glTF, no
               need to add anything if not present. */
            if(const Utility::JsonToken* const gltfNormalTextureScale = gltfNormalTexture->find("scale"_s)) {
                if(!_d->gltf->parseFloat(*gltfNormalTextureScale)) {
                    Error{} << "Trade::GltfImporter::material(): invalid KHR_materials_clearcoat normalTexture scale property";
                    return {};
                }

                arrayAppend(attributes, InPlaceInit,
                    MaterialAttribute::NormalTextureScale,
                    gltfNormalTextureScale->asFloat());
            }
        }
    }

    /* Sort and remove duplicates in remaining extensions */
    const std::size_t uniqueExtensionCount = stableSortRemoveDuplicatesToPrefix(arrayView(gltfExtensionsKeys),
        [](const Utility::JsonToken& a, const Utility::JsonToken& b) {
            return a.asString() < b.asString();
        },
        [](const Utility::JsonToken& a, const Utility::JsonToken& b) {
            return a.asString() == b.asString();
        });

    /* Import unrecognized extension attributes as custom attributes, one
       layer per extension */
    /** @todo use suffix() once it takes suffix size and not prefix size */
    for(const Utility::JsonToken& gltfExtensionKey: gltfExtensionsKeys.exceptPrefix(gltfExtensionsKeys.size() - uniqueExtensionCount)) {
        const Containers::StringView extensionName = gltfExtensionKey.asString();
        if(!extensionName) {
            Warning{} << "Trade::GltfImporter::material(): extension with an empty name, skipping";
            continue;
        }

        CORRADE_INTERNAL_ASSERT(gltfExtensionKey.firstChild());
        const Utility::JsonToken& gltfExtension = *gltfExtensionKey.firstChild();
        /* Token checked for object type already when added to the list */

        /* +1 is the key null byte. +3 are the '#' layer prefix, the layer null
           byte and the length. */
        if(" LayerName"_s.size() + 1 + extensionName.size() + 3 + sizeof(MaterialAttributeType) > sizeof(MaterialAttributeData)) {
            Warning{} << "Trade::GltfImporter::material(): extension name" << extensionName <<
                "is too long with" << extensionName.size() << "characters, skipping";
            continue;
        }

        Containers::Array<Containers::Reference<const Utility::JsonToken>> gltfExtensionKeys;
        for(const Utility::JsonToken& i: gltfExtension.asObject())
            arrayAppend(gltfExtensionKeys, InPlaceInit, i);

        /* Sort and remove duplicates */
        const std::size_t uniqueCount = stableSortRemoveDuplicatesToPrefix(arrayView(gltfExtensionKeys),
            [](const Utility::JsonToken& a, const Utility::JsonToken& b) {
                return a.asString() < b.asString();
            },
            [](const Utility::JsonToken& a, const Utility::JsonToken& b) {
                return a.asString() == b.asString();
            });

        arrayAppend(layers, attributes.size());
        arrayReserve(attributes, attributes.size() + uniqueCount + 1);
        /* Uppercase layer names are reserved. Since all extension names start
           with an uppercase vendor identifier, making the first character
           lowercase seems silly, so we use a unique prefix. */
        arrayAppend(attributes, InPlaceInit, MaterialAttribute::LayerName, Utility::format("#{}", extensionName));
        /** @todo use suffix() once it takes suffix size and not prefix size */
        for(const Utility::JsonToken& gltfKey: gltfExtensionKeys.exceptPrefix(gltfExtensionKeys.size() - uniqueCount)) {
            const Containers::StringView name = gltfKey.asString();
            if(!name) {
                Warning{} << "Trade::GltfImporter::material(): property with an empty name, skipping";
                continue;
            }

            CORRADE_INTERNAL_ASSERT(gltfKey.firstChild());
            const Utility::JsonToken& gltfValue = *gltfKey.firstChild();

            /* Parse glTF textureInfo objects. Any objects without the correct
               suffix and type are ignored. */
            if(gltfValue.type() == Utility::JsonToken::Type::Object) {
                if(name.size() < 8 || !name.hasSuffix("Texture"_s)) {
                    Warning{} << "Trade::GltfImporter::material(): property" << name << "has a non-texture object type, skipping";
                    continue;
                }

                Containers::String nameBuffer = Utility::format("{0}Matrix{0}Coordinates", name);
                if(!materialTexture(gltfValue, attributes,
                    name,
                    nameBuffer.prefix(name.size() + 6),
                    nameBuffer.exceptPrefix(name.size() + 6)))
                {
                    Warning{} << "Trade::GltfImporter::material(): property" << name << "has an invalid texture object, skipping";
                    continue;
                }

                /** @todo If there are ever extensions that reference texture
                    types other than textureInfo and normalTextureInfo, this
                    should instead loop through the texture properties, filter
                    out what's handled by materialTexture() and add the rest,
                    basically same as done for extras */
                if(const Utility::JsonToken* const gltfTextureScale = gltfValue.find("scale"_s)) {
                    if(!_d->gltf->parseFloat(*gltfTextureScale)) {
                        Warning{} << "Trade::GltfImporter::material(): invalid" << extensionName << name << "scale property, skipping";
                        continue;
                    }

                    const Containers::StringView scaleName = nameBuffer.prefix(Utility::formatInto(nameBuffer, "{}Scale", name));
                    if(checkMaterialAttributeSize(scaleName, MaterialAttributeType::Float))
                        arrayAppend(attributes, InPlaceInit,
                            scaleName, gltfTextureScale->asFloat());
                }

            } else {
                /* All other attribute types: bool, numbers, strings */
                if(const Containers::Optional<MaterialAttributeData> parsed = parseMaterialAttribute(*_d->gltf, gltfKey))
                    arrayAppend(attributes, *parsed);
            }
        }
    }

    /* If there's any layer, add the final attribute count */
    arrayAppend(layers, UnsignedInt(attributes.size()));

    /* Can't use growable deleters in a plugin, convert back to the default
       deleter */
    arrayShrink(layers);
    arrayShrink(attributes, DefaultInit);
    return MaterialData{types, std::move(attributes), std::move(layers), &gltfMaterial};
}

UnsignedInt GltfImporter::doTextureCount() const {
    return _d->gltfTextures.size();
}

Int GltfImporter::doTextureForName(const Containers::StringView name) {
    if(!_d->texturesForName) {
        _d->texturesForName.emplace();
        _d->texturesForName->reserve(_d->gltfTextures.size());
        for(std::size_t i = 0; i != _d->gltfTextures.size(); ++i)
            if(const Containers::StringView n = _d->gltfTextures[i].second())
                _d->texturesForName->emplace(n, i);
    }

    const auto found = _d->texturesForName->find(name);
    return found == _d->texturesForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doTextureName(const UnsignedInt id) {
    return _d->gltfTextures[id].second();
}

Containers::Optional<TextureData> GltfImporter::doTexture(const UnsignedInt id) {
    const Utility::JsonToken& gltfTexture = _d->gltfTextures[id].first();

    const Utility::JsonToken* gltfSource = nullptr;

    /* Various extensions, they override the standard image. The core glTF spec
       only allows image/jpeg and image/png and these extend for other MIME
       types. We don't really care as we delegate to AnyImageImporter and let
       it figure out the file type based on magic, so we just pick the first
       available image, assuming that extension order indicates a preference
       and the core image is a fallback if everything else fails. */
    /** @todo Figure out a better priority
        - extensionsRequired?
        - image importers available via manager()->aliasList()?
        - are there even files out there with more than one extension? */
    if(const Utility::JsonToken* const gltfExtensions = gltfTexture.find("extensions"_s)) {
        if(!_d->gltf->parseObject(*gltfExtensions)) {
            Error{} << "Trade::GltfImporter::texture(): invalid extensions property";
            return {};
        }

        /* Pick the first extension we understand */
        for(const Utility::JsonObjectItem i: gltfExtensions->asObject()) {
            const Containers::StringView extensionName = i.key();
            if(
                /* KHR_texture_basisu allows the usage of mimeType image/ktx2
                   but only explicitly talks about KTX2 with Basis compression.
                   We don't care . Note:  but we don't
                   check that either. */
                extensionName != "KHR_texture_basisu"_s &&
                /* GOOGLE_texture_basis is not a registered extension but can
                   be found in some of the early Basis Universal examples.
                   Basis files don't have a registered mimetype either, but as
                   explained above we don't care about mimetype at all. */
                extensionName != "GOOGLE_texture_basis"_s &&
                extensionName != "MSFT_texture_dds"_s
                /** @todo EXT_texture_webp once a plugin provides WebpImporter */
            ) continue;

            if(!_d->gltf->parseObject(i.value())) {
                Error{} << "Trade::GltfImporter::texture(): invalid" << extensionName << "extension";
                return {};
            }

            /* Retrieve the source here already and not in common code below so
               we can include the extension name in the error message. For the
               image index bounds check it's not as important as the offending
               extension can be located from the reported image ID. */
            gltfSource = i.value().find("source"_s);
            if(!gltfSource || !_d->gltf->parseUnsignedInt(*gltfSource)) {
                Error{} << "Trade::GltfImporter::texture(): missing or invalid" << extensionName << "source property";
                return {};
            }

            break;
        }
    }

    if(!gltfSource) {
        gltfSource = gltfTexture.find("source"_s);
        if(!gltfSource || !_d->gltf->parseUnsignedInt(*gltfSource)) {
            Error{} << "Trade::GltfImporter::texture(): missing or invalid source property";
            return {};
        }
    }

    if(gltfSource->asUnsignedInt() >= _d->gltfImages.size()) {
        Error{} << "Trade::GltfImporter::texture(): index" << gltfSource->asUnsignedInt() << "out of range for" << _d->gltfImages.size() << "images";
        return {};
    }

    /* Sampler. If it's not referenced, the specification instructs to use
       "auto filtering and repeat wrapping", i.e. it is left to the implementor
       to decide on the default values... */
    SamplerFilter minificationFilter = SamplerFilter::Linear;
    SamplerFilter magnificationFilter = SamplerFilter::Linear;
    SamplerMipmap mipmap = SamplerMipmap::Linear;
    Math::Vector3<SamplerWrapping> wrapping{SamplerWrapping::Repeat};
    if(const Utility::JsonToken* const gltfSamplerIndex = gltfTexture.find("sampler"_s)) {
        if(!_d->gltf->parseUnsignedInt(*gltfSamplerIndex)) {
            Error{} << "Trade::GltfImporter::texture(): invalid sampler property";
            return {};
        }
        if(gltfSamplerIndex->asUnsignedInt() >= _d->gltfSamplers.size()) {
            Error{} << "Trade::GltfImporter::texture(): index" << gltfSamplerIndex->asUnsignedInt() << "out of range for" << _d->gltfSamplers.size() << "samplers";
            return {};
        }

        Containers::Optional<Document::Sampler>& storage = _d->samplers[gltfSamplerIndex->asUnsignedInt()];
        if(storage) {
            minificationFilter = storage->minificationFilter;
            magnificationFilter = storage->magnificationFilter;
            mipmap = storage->mipmap;
            wrapping = storage->wrapping;
        } else {
            const Utility::JsonToken& gltfSampler = _d->gltfSamplers[gltfSamplerIndex->asUnsignedInt()];

            /* Magnification filter */
            if(const Utility::JsonToken* const gltfMagFilter = gltfSampler.find("magFilter"_s)) {
                if(!_d->gltf->parseUnsignedInt(*gltfMagFilter)) {
                    Error{} << "Trade::GltfImporter::texture(): invalid magFilter property";
                    return {};
                }
                switch(gltfMagFilter->asUnsignedInt()) {
                    case Implementation::GltfFilterNearest:
                        magnificationFilter = SamplerFilter::Nearest;
                        break;
                    case Implementation::GltfFilterLinear:
                        magnificationFilter = SamplerFilter::Linear;
                        break;
                    default:
                        Error{} << "Trade::GltfImporter::texture(): unrecognized magFilter" << gltfMagFilter->asUnsignedInt();
                        return {};
                }
            }

            /* Minification filter */
            if(const Utility::JsonToken* const gltfMinFilter = gltfSampler.find("minFilter"_s)) {
                if(!_d->gltf->parseUnsignedInt(*gltfMinFilter)) {
                    Error{} << "Trade::GltfImporter::texture(): invalid minFilter property";
                    return {};
                }
                switch(gltfMinFilter->asUnsignedInt()) {
                    /* LCOV_EXCL_START */
                    case Implementation::GltfFilterNearest:
                        minificationFilter = SamplerFilter::Nearest;
                        mipmap = SamplerMipmap::Base;
                        break;
                    case Implementation::GltfFilterNearestMipmapNearest:
                        minificationFilter = SamplerFilter::Nearest;
                        mipmap = SamplerMipmap::Nearest;
                        break;
                    case Implementation::GltfFilterNearestMipmapLinear:
                        minificationFilter = SamplerFilter::Nearest;
                        mipmap = SamplerMipmap::Linear;
                        break;
                    case Implementation::GltfFilterLinear:
                        minificationFilter = SamplerFilter::Linear;
                        mipmap = SamplerMipmap::Base;
                        break;
                    case Implementation::GltfFilterLinearMipmapNearest:
                        minificationFilter = SamplerFilter::Linear;
                        mipmap = SamplerMipmap::Nearest;
                        break;
                    case Implementation::GltfFilterLinearMipmapLinear:
                        minificationFilter = SamplerFilter::Linear;
                        mipmap = SamplerMipmap::Linear;
                        break;
                    /* LCOV_EXCL_STOP */
                    default:
                        Error{} << "Trade::GltfImporter::texture(): unrecognized minFilter" << gltfMinFilter->asUnsignedInt();
                        return {};
                }
            }

            /* Wrapping */
            for(const char coordinate: {0, 1}) {
                /* No, I'm definitely not overdoing anything here */
                const char name[]{'w', 'r', 'a', 'p', char('S' + coordinate)};
                if(const Utility::JsonToken* const gltfWrapping = gltfSampler.find({name, sizeof(name)})) {
                    if(!_d->gltf->parseUnsignedInt(*gltfWrapping)) {
                        Error{} << "Trade::GltfImporter::texture(): invalid" << Containers::StringView{name, sizeof(name)} << "property";
                        return {};
                    }
                    switch(gltfWrapping->asUnsignedInt()) {
                        /* LCOV_EXCL_START */
                        case Implementation::GltfWrappingClampToEdge:
                            wrapping[coordinate] = SamplerWrapping::ClampToEdge;
                            break;
                        case Implementation::GltfWrappingMirroredRepeat:
                            wrapping[coordinate] = SamplerWrapping::MirroredRepeat;
                            break;
                        case Implementation::GltfWrappingRepeat:
                            wrapping[coordinate] = SamplerWrapping::Repeat;
                            break;
                        /* LCOV_EXCL_STOP */
                        default:
                            Error{} << "Trade::GltfImporter::texture(): unrecognized" << Containers::StringView{name, sizeof(name)} << gltfWrapping->asUnsignedInt();
                            return {};
                    }
                }
            }

            storage.emplace(
                minificationFilter,
                magnificationFilter,
                mipmap,
                wrapping);
        }
    }

    /* glTF supports only 2D textures */
    return TextureData{TextureType::Texture2D,
        minificationFilter, magnificationFilter,
        mipmap, wrapping, gltfSource->asUnsignedInt(), &gltfTexture};
}

AbstractImporter* GltfImporter::setupOrReuseImporterForImage(const char* const errorPrefix, const UnsignedInt id) {
    /* Looking for the same ID, so reuse an importer populated before. If the
       previous attempt failed, the importer is not set, so return nullptr in
       that case. Going through everything below again would not change the
       outcome anyway, only spam the output with redundant messages. */
    if(_d->imageImporterId == id)
        return _d->imageImporter ? &*_d->imageImporter : nullptr;

    /* Otherwise reset the importer and remember the new ID. If the import
       fails, the importer will stay unset, but the ID will be updated so the
       next round can again just return nullptr above instead of going through
       the doomed-to-fail process again. */
    _d->imageImporter = Containers::NullOpt;
    _d->imageImporterId = id;

    AnyImageImporter importer{*manager()};
    if(fileCallback()) importer.setFileCallback(fileCallback(), fileCallbackUserData());

    const Utility::JsonToken& gltfImage = _d->gltfImages[id].first();

    const Utility::JsonToken* gltfUri = gltfImage.find("uri"_s);
    if(gltfUri && !_d->gltf->parseString(*gltfUri)) {
        Error{} << errorPrefix << "invalid uri property";
        return {};
    }

    const Utility::JsonToken* gltfBufferView = gltfImage.find("bufferView"_s);
    if(gltfBufferView && !_d->gltf->parseUnsignedInt(*gltfBufferView)) {
        Error{} << errorPrefix << "invalid bufferView property";
        return {};
    }

    /* Should have either an uri or a buffer view and not both */
    if(!!gltfUri == !!gltfBufferView) {
        Error{} << errorPrefix << "expected exactly one of uri or bufferView properties defined";
        return {};
    }

    /* Load embedded image. Can either be a buffer view or a base64 payload.
       Buffers are kept in memory until the importer closes but decoded base64
       data is freed after opening the image. */
    if(!gltfUri || isDataUri(gltfUri->asString())) {
        Containers::Optional<Containers::Array<char>> imageData;
        Containers::ArrayView<const void> imageView;

        if(gltfUri) {
            if(!(imageData = loadUri(errorPrefix, gltfUri->asString())))
                return {};
            imageView = *imageData;

        } else if(gltfBufferView) {
            const Containers::Optional<Containers::Triple<Containers::ArrayView<const char>, UnsignedInt, UnsignedInt>> bufferView = parseBufferView(errorPrefix, gltfBufferView->asUnsignedInt());
            if(!bufferView) return {};

            /* 3.6.1.1. (Binary Data Storage § Buffers and Buffer Views §
               Overview) says "Buffer views with [non-vertex] types of data
               MUST NOT not define byteStride", which makes sense */
            if(bufferView->second()) {
                Error{} << errorPrefix << "buffer view" << gltfBufferView->asUnsignedInt() << "is strided";
                return {};
            }

            imageView = bufferView->first();

        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

        if(!importer.openData(imageView))
            return nullptr;
        return &_d->imageImporter.emplace(std::move(importer));
    }

    /* Load external image */
    if(!_d->filename && !fileCallback()) {
        Error{} << errorPrefix << "external images can be imported only when opening files from the filesystem or if a file callback is present";
        return nullptr;
    }

    const Containers::Optional<Containers::String> decodedUri = decodeUri(errorPrefix, gltfUri->asString());
    if(!decodedUri)
        return nullptr;
    if(!importer.openFile(Utility::Path::join(_d->filename ? Utility::Path::split(*_d->filename).first() : "", *decodedUri)))
        return nullptr;
    return &_d->imageImporter.emplace(std::move(importer));
}

UnsignedInt GltfImporter::doImage2DCount() const {
    return _d->gltfImages.size();
}

Int GltfImporter::doImage2DForName(const Containers::StringView name) {
    if(!_d->imagesForName) {
        _d->imagesForName.emplace();
        _d->imagesForName->reserve(_d->gltfImages.size());
        for(std::size_t i = 0; i != _d->gltfImages.size(); ++i)
            if(const Containers::StringView n = _d->gltfImages[i].second())
                _d->imagesForName->emplace(n, i);
    }

    const auto found = _d->imagesForName->find(name);
    return found == _d->imagesForName->end() ? -1 : found->second;
}

Containers::String GltfImporter::doImage2DName(const UnsignedInt id) {
    return _d->gltfImages[id].second();
}

UnsignedInt GltfImporter::doImage2DLevelCount(const UnsignedInt id) {
    CORRADE_ASSERT(manager(), "Trade::GltfImporter::image2DLevelCount(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    AbstractImporter* importer = setupOrReuseImporterForImage("Trade::GltfImporter::image2DLevelCount():", id);
    /* image2DLevelCount() isn't supposed to fail (image2D() is, instead), so
       report 1 on failure and expect image2D() to fail later */
    if(!importer) return 1;

    return importer->image2DLevelCount(0);
}

Containers::Optional<ImageData2D> GltfImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    CORRADE_ASSERT(manager(), "Trade::GltfImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to load images", {});

    AbstractImporter* importer = setupOrReuseImporterForImage("Trade::GltfImporter::image2D():", id);
    if(!importer) return {};

    /* Include a pointer to the glTF image in the result */
    Containers::Optional<ImageData2D> imageData = importer->image2D(0, level);
    if(!imageData) return Containers::NullOpt;
    return ImageData2D{std::move(*imageData), &*_d->gltfImages[id].first()};
}

const void* GltfImporter::doImporterState() const {
    return &*_d->gltf;
}

}}

CORRADE_PLUGIN_REGISTER(GltfImporter, Magnum::Trade::GltfImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
