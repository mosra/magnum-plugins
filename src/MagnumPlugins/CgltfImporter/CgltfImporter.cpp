/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

#include "CgltfImporter.h"

#include <algorithm> /* std::stable_sort() */
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayTuple.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/MurmurHash2.h>
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

/* Cgltf doesn't load .glb on big-endian correctly:
   https://github.com/jkuhlmann/cgltf/issues/150
   Even if we patched .glb files in memory, we'd still need to convert all
   buffers from little-endian. Adds a lot of complexity, and not testable. */
#ifdef CORRADE_TARGET_BIG_ENDIAN
#error big-endian systems are not supported by cgltf
#endif

/* Since we set custom allocator callbacks for cgltf_parse, we can override
   CGLTF_MALLOC / CGLTF_FREE to assert that the default allocation functions
   aren't called */
namespace {
    /* LCOV_EXCL_START */
    void* mallocNoop(size_t) { CORRADE_INTERNAL_ASSERT_UNREACHABLE(); }
    void freeNoop(void*) { CORRADE_INTERNAL_ASSERT_UNREACHABLE(); }
    /* LCOV_EXCL_STOP */
}
#define CGLTF_MALLOC(size) mallocNoop(size)
#define CGLTF_FREE(ptr) freeNoop(ptr)
/* If we had a good replacement for ato(i|f|ll) we could set the corresponding
   CGLTF_ATOI etc. here and prevent stdlib.h from being included in cgltf.h */

#define CGLTF_IMPLEMENTATION

#include <cgltf.h>

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

/* Convert cgltf type enums back into strings for useful error output */
Containers::StringView gltfTypeName(cgltf_type type) {
    switch(type) {
        case cgltf_type_scalar: return "SCALAR"_s;
        case cgltf_type_vec2:   return "VEC2"_s;
        case cgltf_type_vec3:   return "VEC3"_s;
        case cgltf_type_vec4:   return "VEC4"_s;
        case cgltf_type_mat2:   return "MAT2"_s;
        case cgltf_type_mat3:   return "MAT3"_s;
        case cgltf_type_mat4:   return "MAT4"_s;
        case cgltf_type_invalid:
            break;
    }

    return "UNKNOWN"_s;
}

Containers::StringView gltfComponentTypeName(cgltf_component_type type) {
    switch(type) {
        case cgltf_component_type_r_8:   return "BYTE (5120)"_s;
        case cgltf_component_type_r_8u:  return "UNSIGNED_BYTE (5121)"_s;
        case cgltf_component_type_r_16:  return "SHORT (5122)"_s;
        case cgltf_component_type_r_16u: return "UNSIGNED_SHORT (5123)"_s;
        case cgltf_component_type_r_32u: return "UNSIGNED_INT (5125)"_s;
        case cgltf_component_type_r_32f: return "FLOAT (5126)"_s;
        case cgltf_component_type_invalid:
            break;
    }

    return "UNKNOWN"_s;
}

std::size_t elementSize(const cgltf_accessor* accessor) {
    /* Technically cgltf_calc_size isn't part of the public API but we bundle
       cgltf so there shouldn't be any surprises. Worst case we'll have to copy
       its content from an old version if it gets removed. */
    return cgltf_calc_size(accessor->type, accessor->component_type);
}

bool checkAccessor(const cgltf_data* data, const char* const function, const cgltf_accessor* accessor) {
    CORRADE_INTERNAL_ASSERT(accessor);
    const UnsignedInt accessorId = accessor - data->accessors;

    /** @todo Validate alignment rules, calculate correct stride in accessorView():
        https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#data-alignment */

    if(accessor->is_sparse) {
        Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): accessor" << accessorId << "is using sparse storage, which is unsupported";
        return false;
    }
    /* Buffer views are optional in accessors, we're supposed to fill the view
       with zeros. Only makes sense with sparse data and we don't support
       that. */
    if(!accessor->buffer_view) {
        Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): accessor" << accessorId << "has no buffer view";
        return false;
    }

    const cgltf_buffer_view* bufferView = accessor->buffer_view;
    const UnsignedInt bufferViewId = bufferView - data->buffer_views;
    const cgltf_buffer* buffer = bufferView->buffer;

    const std::size_t typeSize = elementSize(accessor);
    const std::size_t requiredBufferViewSize = accessor->offset + accessor->stride*(accessor->count - 1) + typeSize;
    if(bufferView->size < requiredBufferViewSize) {
        Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): accessor" << accessorId << "needs" << requiredBufferViewSize << "bytes but buffer view" << bufferViewId << "has only" << bufferView->size;
        return false;
    }

    const std::size_t requiredBufferSize = bufferView->offset + bufferView->size;
    if(buffer->size < requiredBufferSize) {
        const UnsignedInt bufferId = buffer - data->buffers;
        Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): buffer view" << bufferViewId << "needs" << requiredBufferSize << "bytes but buffer" << bufferId << "has only" << buffer->size;
        return false;
    }

    /* Cgltf copies the bufferview stride into the accessor. If that's zero, it
       copies the element size into the stride. */
    if(accessor->stride < typeSize) {
        Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "():" << typeSize << Debug::nospace << "-byte type defined by accessor" << accessorId << "can't fit into buffer view" << bufferViewId << "stride of" << accessor->stride;
        return false;
    }

    return true;
}

/* Data URI according to RFC 2397 */
bool isDataUri(Containers::StringView uri) {
    return uri.hasPrefix("data:"_s);
}

/* Decode percent-encoded characters in URIs:
   https://datatracker.ietf.org/doc/html/rfc3986#section-2.1 */
std::string decodeUri(Containers::StringView uri) {
    std::string decoded = uri;
    const std::size_t decodedSize = cgltf_decode_uri(&decoded[0]);
    decoded.resize(decodedSize);

    return decoded;
}

Containers::Array<jsmntok_t> parseJson(Containers::StringView str) {
    jsmn_parser parser{0, 0, 0};
    Int numTokens = jsmn_parse(&parser, str.data(), str.size(), nullptr, 0);
    /* All JSON strings we're parsing come from cgltf and should already have
       passed jsmn parsing */
    CORRADE_INTERNAL_ASSERT(numTokens >= 0);

    Containers::Array<jsmntok_t> tokens{std::size_t(numTokens)};
    jsmn_init(&parser);
    numTokens = jsmn_parse(&parser, str.data(), str.size(), tokens.data(), numTokens);
    CORRADE_INTERNAL_ASSERT(std::size_t(numTokens) == tokens.size());

    return tokens;
}

Containers::StringView tokenString(Containers::StringView json, const jsmntok_t& token) {
    return json.slice(token.start, token.end);
}

}

struct CgltfImporter::Document {
    ~Document();

    Containers::Optional<std::string> filePath;
    Containers::Array<char> fileData;

    cgltf_options options;
    cgltf_data* data = nullptr;

    Containers::Optional<Containers::ArrayView<const char>> loadUri(Containers::StringView uri, Containers::Array<char>& storage, const char* const function);
    bool loadBuffer(UnsignedInt id, const char* const function);
    Containers::Optional<Containers::StridedArrayView2D<const char>> accessorView(const cgltf_accessor* accessor, const char* const function);

    /* Storage for buffer content if the user set no file callback or a buffer
       is embedded as base64. These are filled on demand. We don't check for
       duplicate URIs since that's incredibly unlikely and hard to get right,
       so the buffer id is used as the index. */
    Containers::Array<Containers::Array<char>> bufferData;

    /* Cgltf's JSON parser jsmn doesn't decode escaped characters so we do it
       after parsing. If there's nothing to escape, returns a view on the
       original string. Decoded strings are cached in a map indexed by the
       input view data pointer. This works because we only call this function
       with views on strings from cgltf_data.

       Note that parsing inside cgltf happens with unescaped strings, but we
       have no influence on that. In practice, this shouldn't be a problem. Old
       versions of the spec used to explicitly forbid non-ASCII keys/enums:
       https://github.com/KhronosGroup/glTF/tree/fd3ab461a1114fb0250bd76099153d2af50a7a1d/specification/2.0#json-encoding
       Newer spec versions changed this to "ASCII characters [...] SHOULD be
       written without JSON escaping" */
    Containers::StringView decodeString(Containers::StringView str);

    std::unordered_map<const char*, const Containers::String> decodedStrings;

    /* We can use StringView as the map key here because all underlying strings
       won't go out of scope while a file is opened. They either point to the
       original name strings in cgltf_data or to decodedStrings. */
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
       be too annoying to test. Also, assuming the importer knows all builtin
       names, in most cases these would be empty anyway. */
    std::unordered_map<Containers::StringView, MeshAttribute>
        meshAttributesForName;
    Containers::Array<Containers::StringView> meshAttributeNames;

    /* Mapping for multi-primitive meshes:

        -   meshMap.size() is the count of meshes reported to the user
        -   meshSizeOffsets.size() is the count of original meshes in the file
        -   meshMap[id] is a pair of (original mesh ID, primitive ID)
        -   meshSizeOffsets[j] points to the first item in meshMap for
            original mesh ID `j` -- which also translates the original ID to
            reported ID
        -   meshSizeOffsets[j + 1] - meshSizeOffsets[j] is count of meshes for
            original mesh ID `j` (or number of primitives in given mesh)
    */
    Containers::Array<Containers::Pair<std::size_t, std::size_t>> meshMap;
    Containers::Array<std::size_t> meshSizeOffsets;

    /* Mapping for nodes having multi-primitive nodes. The same as above, but
       for nodes. Hierarchy-wise, the subsequent nodes are direct children of
       the first, have no transformation or other children and point to the
       subsequent meshes. */
    Containers::Array<Containers::Pair<std::size_t, std::size_t>> nodeMap;
    Containers::Array<std::size_t> nodeSizeOffsets;

    /* If a file contains texture coordinates that are not floats or normalized
       in the 0-1, the textureCoordinateYFlipInMaterial option is enabled
       implicitly as we can't perform Y-flip directly on the data. */
    bool textureCoordinateYFlipInMaterial = false;

    void materialTexture(const cgltf_texture_view& texture, Containers::Array<MaterialAttributeData>& attributes, Containers::StringView attribute, Containers::StringView matrixAttribute, Containers::StringView coordinateAttribute) const;

    bool open = false;

    UnsignedInt imageImporterId = ~UnsignedInt{};
    Containers::Optional<AnyImageImporter> imageImporter;
};

CgltfImporter::Document::~Document() {
    if(data) cgltf_free(data);
}

Containers::Optional<Containers::ArrayView<const char>> CgltfImporter::Document::loadUri(Containers::StringView uri, Containers::Array<char>& storage, const char* const function) {
    const AbstractImporter& importer = *static_cast<AbstractImporter*>(options.file.user_data);

    if(isDataUri(uri)) {
        /* Data URI with base64 payload according to RFC 2397:
           data:[<mediatype>][;base64],<data> */
        Containers::StringView base64;
        const Containers::Array3<Containers::StringView> parts = uri.partition(',');

        /* Non-base64 data URIs are allowed by RFC 2397, but make no sense for
           glTF. cgltf_load_buffers doesn't allow them, either. */
        if(parts.front().hasSuffix(";base64"_s)) {
            /* This will be empty for both a missing comma and an empty payload */
            base64 = parts.back();
        }

        if(base64.isEmpty()) {
            Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): data URI has no base64 payload";
            return Containers::NullOpt;
        }

        /* Decoded size. For some reason cgltf_load_buffer_base64 doesn't take
           the string length as input, and fails if it finds a padding
           character. */
        const std::size_t padding = base64.size() - base64.trimmedSuffix("="_s).size();
        const std::size_t size = base64.size()/4*3 - padding;

        /* cgltf_load_buffer_base64 will allocate using the memory callbacks
           set in doOpenData() which use new char[] and delete[]. We can wrap
           that memory in an Array with the default deleter. */
        void* decoded = nullptr;
        const cgltf_result result = cgltf_load_buffer_base64(&options, size, base64.data(), &decoded);
        if(result != cgltf_result_success) {
            Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): invalid base64 string in data URI";
            return Containers::NullOpt;
        }
        CORRADE_INTERNAL_ASSERT(decoded);
        storage = Containers::Array<char>{static_cast<char*>(decoded), size};
        return Containers::arrayCast<const char>(storage);
    } else if(importer.fileCallback()) {
        const std::string fullPath = Utility::Directory::join(filePath ? *filePath : "", decodeUri(decodeString(uri)));
        Containers::Optional<Containers::ArrayView<const char>> view = importer.fileCallback()(fullPath, InputFileCallbackPolicy::LoadPermanent, importer.fileCallbackUserData());
        if(!view) {
            Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): error opening file:" << Containers::StringView{fullPath} << ": file callback failed";
            return Containers::NullOpt;
        }
        return *view;
    } else {
        if(!filePath) {
            Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): external buffers can be imported only when opening files from the filesystem or if a file callback is present";
            return Containers::NullOpt;
        }
        const std::string fullPath = Utility::Directory::join(*filePath, decodeUri(decodeString(uri)));
        if(!Utility::Directory::exists(fullPath)) {
            Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): error opening file:" << Containers::StringView{fullPath} << ": file not found";
            return Containers::NullOpt;
        }
        storage = Utility::Directory::read(fullPath);
        return Containers::arrayCast<const char>(storage);
    }
}

bool CgltfImporter::Document::loadBuffer(UnsignedInt id, const char* const function) {
    CORRADE_INTERNAL_ASSERT(id < data->buffers_count);
    cgltf_buffer& buffer = data->buffers[id];
    if(buffer.data)
        return true;

    Containers::ArrayView<const char> view;
    if(!buffer.uri) {
        /* URI may only be empty for buffers referencing the glb binary blob */
        if(id != 0 || !data->bin) {
            Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "():" <<
                "buffer" << id << "has no URI";
            return false;
        }
        view = Containers::arrayView(static_cast<const char*>(data->bin), data->bin_size);
    } else {
        const auto loaded = loadUri(buffer.uri, bufferData[id], function);
        if(!loaded)
            return false;
        view = *loaded;
    }

    /* The spec mentions that non-GLB buffer length can be greater than
       byteLength. GLB buffer chunks may also be up to 3 bytes larger than
       byteLength because of padding. So we can't check for equality. */
    if(view.size() < buffer.size) {
        Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "():" <<
            "buffer" << id << "is too short, expected" << buffer.size <<
            "bytes but got" << view.size();
        return false;
    }

    buffer.data = const_cast<char*>(view.data()); /* sigh */
    /* Tell cgltf not to free buffer.data in cgltf_free */
    buffer.data_free_method = cgltf_data_free_method_none;

    return true;
}

Containers::Optional<Containers::StridedArrayView2D<const char>> CgltfImporter::Document::accessorView(const cgltf_accessor* accessor, const char* const function) {
    /* All this assumes the accessor was checked using checkAccessor() */
    const cgltf_buffer_view* bufferView = accessor->buffer_view;
    const cgltf_buffer* buffer = bufferView->buffer;
    const UnsignedInt bufferId = buffer - data->buffers;
    if(!loadBuffer(bufferId, function))
        return Containers::NullOpt;

    return Containers::StridedArrayView2D<const char>{Containers::arrayView(buffer->data, buffer->size),
        reinterpret_cast<const char*>(buffer->data) + bufferView->offset + accessor->offset,
        {accessor->count, elementSize(accessor)},
        {std::ptrdiff_t(accessor->stride), 1}};
}

Containers::StringView CgltfImporter::Document::decodeString(Containers::StringView str) {
    if(str.isEmpty())
        return str;

    /* String has been decoded before */
    const auto found = decodedStrings.find(str.data());
    if(found != decodedStrings.end())
        return found->second;

    /* The input string can be UTF-8 encoded but we can use a byte search here
       since all multi-byte UTF-8 characters have the high bit set and '\\'
       doesn't, so this will only match single-byte ASCII characters. */
    Containers::StringView escape = str.find('\\');
    /* No escaped sequence found. If the view is null-terminated (all strings
       in cgltf_data should be), this stores a non-owning String. */
    if(escape.isEmpty())
        return decodedStrings.emplace(str.data(), Containers::String::nullTerminatedView(str)).first->second;

    /* Skip any processing until the first escape character */
    const std::size_t start = escape.data() - str.data();

    Containers::String decoded{str};
    const std::size_t decodedSize = cgltf_decode_string(decoded.data() + start) + start;
    CORRADE_INTERNAL_ASSERT(decodedSize < str.size());

    return decodedStrings.emplace(str.data(), Containers::String{decoded.prefix(decodedSize)}).first->second;
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

CgltfImporter::CgltfImporter() {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

CgltfImporter::CgltfImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

CgltfImporter::CgltfImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter{manager} {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

CgltfImporter::~CgltfImporter() = default;

ImporterFeatures CgltfImporter::doFeatures() const { return ImporterFeature::OpenData|ImporterFeature::FileCallback; }

bool CgltfImporter::doIsOpened() const { return !!_d && _d->open; }

void CgltfImporter::doClose() { _d = nullptr; }

void CgltfImporter::doOpenFile(const std::string& filename) {
    _d.reset(new Document);
    _d->filePath = Utility::Directory::path(filename);
    AbstractImporter::doOpenFile(filename);
}

void CgltfImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    if(!_d) _d.reset(new Document);

    /* Copy file content. Take over the existing array or copy the data if we
       can't. We need to keep the data around for .glb binary blobs and
       extension data which cgltf stores as pointers into the original memory
       passed to cgltf_parse. */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        _d->fileData = std::move(data);
    } else {
        _d->fileData = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, _d->fileData);
    }

    /* Auto-detect glb/gltf */
    _d->options.type = cgltf_file_type::cgltf_file_type_invalid;
    /* Determine json token count to allocate (by parsing twice) */
    _d->options.json_token_count = 0;

    /* Set up memory callbacks. The default memory callbacks (when set to
       nullptr) use malloc and free. Prefer using new and delete, allows us to
       use the default deleter when wrapping memory in Array, and it'll throw
       bad_alloc if allocation fails. */
    _d->options.memory.alloc = [](void*, cgltf_size size) -> void* { return new char[size]; };
    _d->options.memory.free = [](void*, void* ptr) { delete[] static_cast<char*>(ptr); };
    _d->options.memory.user_data = nullptr;

    /* The file callbacks are only needed for cgltf_load_buffers which we don't
       call, but we still replace the default ones to assert that they're
       never called. Unfortunately, this doesn't prevent cgltf from linking to
       stdio anyway. */
    _d->options.file.read = [](const cgltf_memory_options*, const cgltf_file_options*, const char*, cgltf_size*, void**) -> cgltf_result {
        CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    };
    _d->options.file.release = [](const cgltf_memory_options*, const cgltf_file_options*, void* ptr) -> void {
        /* cgltf_free calls this function with a nullptr file_data that's only
           set when using cgltf_parse_file */
        if(ptr == nullptr) return;
        CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    };
    _d->options.file.user_data = this;

    /* Parse file, without loading or decoding buffers/images */
    const cgltf_result result = cgltf_parse(&_d->options, _d->fileData.data(), _d->fileData.size(), &_d->data);

    /* A general note on error checking in cgltf:
       - cgltf_parse fails if any index is out of bounds, mandatory or not
       - cgltf_parse fails if a mandatory property is missing
       - optional properties are set to the spec-mandated default value (if
         there is one), 0 or nullptr (if they're indices).

       We're not using cgltf_validate() because the error granularity is rather
       underwhelming. All of its relevant checks are implemented on our side,
       allowing us to delay them to when they're needed, e.g. accessor and
       buffer size. */
    if(result != cgltf_result_success) {
        const char* error{};
        switch(result) {
            /* This can also be returned for arrays with too many items before
               any allocation happens, so we can't quite ignore it. Rather
               impossible to test, however. */
            case cgltf_result_out_of_memory:
                error = "out of memory";
                break;
            case cgltf_result_unknown_format:
                error = "unknown binary glTF format";
                break;
            case cgltf_result_invalid_json:
                error = "invalid JSON";
                break;
            case cgltf_result_invalid_gltf:
                error = "invalid glTF, usually caused by invalid indices or missing required attributes";
                break;
            case cgltf_result_legacy_gltf:
                error = "legacy glTF version";
                break;
            case cgltf_result_data_too_short:
                error = "data too short";
                break;
            /* LCOV_EXCL_START */
            /* Only returned from cgltf's default file callback */
            case cgltf_result_file_not_found:
            /* Only returned by cgltf_load_buffer_base64 and cgltf's default
               file callback */
            case cgltf_result_io_error:
            /* We passed a nullptr somewhere, this should never happen */
            case cgltf_result_invalid_options:
            default:
                CORRADE_INTERNAL_ASSERT_UNREACHABLE();
            /* LCOV_EXCL_STOP */
        }

        Error{} << "Trade::CgltfImporter::openData(): error opening file:" << error;
        doClose();
        return;
    }

    CORRADE_INTERNAL_ASSERT(_d->data != nullptr);

    /* Major versions are forward- and backward-compatible, but minVersion can
       be used to require support for features added in new minor versions.
       So far there's only 2.0 so we can use an exact comparison.
       cgltf already checked that asset.version >= 2.0 (if it exists). */
    const cgltf_asset& asset = _d->data->asset;
    if(asset.min_version && asset.min_version != "2.0"_s) {
        Error{} << "Trade::CgltfImporter::openData(): unsupported minVersion" << asset.min_version << Debug::nospace << ", expected 2.0";
        doClose();
        return;
    }
    if(asset.version && !Containers::StringView{asset.version}.hasPrefix("2."_s)) {
        Error{} << "Trade::CgltfImporter::openData(): unsupported version" << asset.version << Debug::nospace << ", expected 2.x";
        doClose();
        return;
    }

    /* Check required extensions. Every extension in extensionsRequired is
       required to "load and/or render an asset". */
    const bool ignoreRequiredExtensions = configuration().value<bool>("ignoreRequiredExtensions");

    constexpr Containers::StringView supportedExtensions[]{
        /* Parsed by cgltf and handled by us */
        "KHR_lights_punctual"_s,
        "KHR_materials_clearcoat"_s,
        "KHR_materials_pbrSpecularGlossiness"_s,
        "KHR_materials_unlit"_s,
        "KHR_mesh_quantization"_s,
        "KHR_texture_basisu"_s,
        "KHR_texture_transform"_s,
        /* Manually parsed */
        "GOOGLE_texture_basis"_s,
        "MSFT_texture_dds"_s
    };

    /* M*N loop should be okay here, extensionsRequired should usually have no
       or very few entries. Consider binary search if the list of supported
       extensions reaches a few dozen. */
    for(Containers::StringView required: Containers::arrayView(_d->data->extensions_required, _d->data->extensions_required_count)) {
        bool found = false;
        for(const auto& supported: supportedExtensions) {
            if(supported == required) {
                found = true;
                break;
            }
        }

        if(!found) {
            if(ignoreRequiredExtensions) {
                Warning{} << "Trade::CgltfImporter::openData(): required extension" << required << "not supported";
            } else {
                Error{} << "Trade::CgltfImporter::openData(): required extension" << required << "not supported";
                doClose();
                return;
            }
        }
    }

    /* Find cycles in node tree */
    for(std::size_t i = 0; i != _d->data->nodes_count; ++i) {
        const cgltf_node* p1 = _d->data->nodes[i].parent;
        const cgltf_node* p2 = p1 ? p1->parent : nullptr;

        while(p1 && p2) {
            if(p1 == p2) {
                Error{} << "Trade::CgltfImporter::openData(): node tree contains cycle starting at node" << i;
                doClose();
                return;
            }

            p1 = p1->parent;
            p2 = p2->parent ? p2->parent->parent : nullptr;
        }
    }

    /* Treat meshes with multiple primitives as separate meshes. Each mesh gets
       duplicated as many times as is the size of the primitives array. */
    Containers::arrayReserve(_d->meshMap, _d->data->meshes_count);
    _d->meshSizeOffsets = Containers::Array<std::size_t>{_d->data->meshes_count + 1};

    _d->meshSizeOffsets[0] = 0;
    for(std::size_t i = 0; i != _d->data->meshes_count; ++i) {
        const std::size_t count = _d->data->meshes[i].primitives_count;
        CORRADE_INTERNAL_ASSERT(count > 0);
        for(std::size_t j = 0; j != count; ++j)
            arrayAppend(_d->meshMap, InPlaceInit, i, j);

        _d->meshSizeOffsets[i + 1] = _d->meshMap.size();
    }

    /* In order to support multi-primitive meshes, we need to duplicate the
       nodes as well */
    Containers::arrayReserve(_d->nodeMap, _d->data->nodes_count);
    _d->nodeSizeOffsets = Containers::Array<std::size_t>{_d->data->nodes_count + 1};

    _d->nodeSizeOffsets[0] = 0;
    for(std::size_t i = 0; i != _d->data->nodes_count; ++i) {
        const cgltf_mesh* mesh = _d->data->nodes[i].mesh;
        /* If a node has a mesh with multiple primitives, add nested nodes
           containing the other primitives after it */
        const std::size_t count = mesh ? mesh->primitives_count : 1;
        for(std::size_t j = 0; j != count; ++j)
            arrayAppend(_d->nodeMap, InPlaceInit, i, j);

        _d->nodeSizeOffsets[i + 1] = _d->nodeMap.size();
    }

    /* Go through all meshes, collect custom attributes and decide about
       implicitly enabling textureCoordinateYFlipInMaterial if it isn't already
       requested from the configuration and there are any texture coordinates
       that need it */
    if(configuration().value<bool>("textureCoordinateYFlipInMaterial"))
        _d->textureCoordinateYFlipInMaterial = true;
    for(const cgltf_mesh& mesh: Containers::arrayView(_d->data->meshes, _d->data->meshes_count)) {
        for(const cgltf_primitive& primitive: Containers::arrayView(mesh.primitives, mesh.primitives_count)) {
            for(const cgltf_attribute& attribute: Containers::arrayView(primitive.attributes, primitive.attributes_count)) {
                if(attribute.type == cgltf_attribute_type_texcoord) {
                    if(!_d->textureCoordinateYFlipInMaterial) {
                        const cgltf_component_type type = attribute.data->component_type;
                        const bool normalized = attribute.data->normalized;
                        if(type == cgltf_component_type_r_8 ||
                           type == cgltf_component_type_r_16 ||
                          (type == cgltf_component_type_r_8u && !normalized) ||
                          (type == cgltf_component_type_r_16u && !normalized)) {
                            Debug{} << "Trade::CgltfImporter::openData(): file contains non-normalized texture coordinates, implicitly enabling textureCoordinateYFlipInMaterial";
                            _d->textureCoordinateYFlipInMaterial = true;
                        }
                    }

                /* If the name isn't recognized or not in MeshAttribute, add
                   the attribute to custom if not there already */
                } else if(attribute.type != cgltf_attribute_type_position &&
                    attribute.type != cgltf_attribute_type_normal &&
                    attribute.type != cgltf_attribute_type_tangent &&
                    attribute.type != cgltf_attribute_type_color)
                {
                    /* Get the semantic base name ([semantic]_[set_index]) for
                       known attributes that are not supported in MeshAttribute
                       (JOINTS_n and WEIGHTS_n). This lets us group multiple
                       sets to the same attribute.
                       For unknown/user-defined attributes all name formats are
                       allowed and we don't attempt to group them. */
                    /** @todo Remove all this once Magnum adds these to MeshAttribute
                       (pending https://github.com/mosra/magnum/pull/441) */
                    const Containers::StringView name{attribute.name};
                    const Containers::StringView semantic = attribute.type != cgltf_attribute_type_invalid ?
                        name.partition('_')[0] : name;

                    /* The spec says that all user-defined attributes must
                       start with an underscore. We don't really care and just
                       print a warning. */
                    if(attribute.type == cgltf_attribute_type_invalid && !name.hasPrefix("_"_s))
                        Warning{} << "Trade::CgltfImporter::openData(): unknown attribute" << name << Debug::nospace << ", importing as custom attribute";

                    if(_d->meshAttributesForName.emplace(semantic,
                        meshAttributeCustom(_d->meshAttributeNames.size())).second)
                        arrayAppend(_d->meshAttributeNames, semantic);
                }
            }
        }
    }

    _d->open = true;

    /* Buffers are loaded on demand, but we need to prepare the storage array */
    _d->bufferData = Containers::Array<Containers::Array<char>>{_d->data->buffers_count};

    /* Name maps are lazy-loaded because these might not be needed every time */
}

UnsignedInt CgltfImporter::doAnimationCount() const {
    /* If the animations are merged, there's at most one */
    if(configuration().value<bool>("mergeAnimationClips"))
        return _d->data->animations_count == 0 ? 0 : 1;

    return _d->data->animations_count;
}

Int CgltfImporter::doAnimationForName(const std::string& name) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return -1;

    if(!_d->animationsForName) {
        _d->animationsForName.emplace();
        _d->animationsForName->reserve(_d->data->animations_count);
        for(std::size_t i = 0; i != _d->data->animations_count; ++i)
            _d->animationsForName->emplace(_d->decodeString(_d->data->animations[i].name), i);
    }

    const auto found = _d->animationsForName->find(name);
    return found == _d->animationsForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doAnimationName(UnsignedInt id) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return {};
    return _d->decodeString(_d->data->animations[id].name);
}

namespace {

template<class V> void postprocessSplineTrack(const cgltf_accessor* timeTrackUsed, const Containers::ArrayView<const Float> keys, const Containers::ArrayView<Math::CubicHermite<V>> values) {
    /* Already processed, don't do that again */
    if(timeTrackUsed != nullptr) return;

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

Containers::Optional<AnimationData> CgltfImporter::doAnimation(UnsignedInt id) {
    /* Import either a single animation or all of them together. At the moment,
       Blender doesn't really support cinematic animations (affecting multiple
       objects): https://blender.stackexchange.com/q/5689. And since
       https://github.com/KhronosGroup/glTF-Blender-Exporter/pull/166, these
       are exported as a set of object-specific clips, which may not be wanted,
       so we give the users an option to merge them all together. */
    const std::size_t animationBegin =
        configuration().value<bool>("mergeAnimationClips") ? 0 : id;
    const std::size_t animationEnd =
        configuration().value<bool>("mergeAnimationClips") ? _d->data->animations_count : id + 1;

    const Containers::ArrayView<cgltf_animation> animations = Containers::arrayView(
        _d->data->animations + animationBegin, animationEnd - animationBegin);

    /* First gather the input and output data ranges. Key is unique accessor
       pointer so we don't duplicate shared data, value is range in the input
       buffer, offset in the output data and pointer of the corresponding key
       track in case given track is a spline interpolation. The key pointer is
       initialized to nullptr and will be used later to check that a spline
       track was not used with more than one time track, as it needs to be
       postprocessed for given time track. */
    struct SamplerData {
        Containers::StridedArrayView2D<const char> src;
        std::size_t outputOffset;
        const cgltf_accessor* timeTrack;
    };
    std::unordered_map<const cgltf_accessor*, SamplerData> samplerData;
    std::size_t dataSize = 0;
    for(const cgltf_animation& animation: animations) {
        for(std::size_t i = 0; i != animation.samplers_count; ++i) {
            const cgltf_animation_sampler& sampler = animation.samplers[i];

            /** @todo handle alignment once we do more than just four-byte types */

            /* If the input view is not yet present in the output data buffer,
               add it */
            if(samplerData.find(sampler.input) == samplerData.end()) {
                if(!checkAccessor(_d->data, "animation", sampler.input))
                    return Containers::NullOpt;
                Containers::Optional<Containers::StridedArrayView2D<const char>> view = _d->accessorView(sampler.input, "animation");
                if(!view)
                    return Containers::NullOpt;

                samplerData.emplace(sampler.input, SamplerData{*view, dataSize, nullptr});
                dataSize += view->size()[0]*view->size()[1];
            }

            /* If the output view is not yet present in the output data buffer,
               add it */
            if(samplerData.find(sampler.output) == samplerData.end()) {
                if(!checkAccessor(_d->data, "animation", sampler.output))
                    return Containers::NullOpt;
                Containers::Optional<Containers::StridedArrayView2D<const char>> view = _d->accessorView(sampler.output, "animation");
                if(!view)
                    return Containers::NullOpt;

                samplerData.emplace(sampler.output, SamplerData{*view, dataSize, nullptr});
                dataSize += view->size()[0]*view->size()[1];
            }
        }
    }

    /* Populate the data array */
    /**
     * @todo Once memory-mapped files are supported, this can all go away
     *      except when spline tracks are present -- in that case we need to
     *      postprocess them and can't just use the memory directly.
     */
    Containers::Array<char> data{dataSize};
    for(const std::pair<const cgltf_accessor* const, SamplerData>& view: samplerData) {
        Containers::StridedArrayView2D<const char> src = view.second.src;
        Containers::StridedArrayView2D<char> dst{data.suffix(view.second.outputOffset),
            src.size()};
        Utility::copy(src, dst);
    }

    /* Calculate total track count. If merging all animations together, this is
       the sum of all clip track counts. */
    std::size_t trackCount = 0;
    for(const cgltf_animation& animation : animations) {
        for(std::size_t i = 0; i != animation.channels_count; ++i) {
            /* Skip animations without a target node. See comment below. */
            if(animation.channels[i].target_node)
                ++trackCount;
        }
    }

    /* Import all tracks */
    bool hadToRenormalize = false;
    std::size_t trackId = 0;
    Containers::Array<Trade::AnimationTrackData> tracks{trackCount};
    for(const cgltf_animation& animation : animations) {
        for(std::size_t i = 0; i != animation.channels_count; ++i) {
            const cgltf_animation_channel& channel = animation.channels[i];
            const cgltf_animation_sampler& sampler = *channel.sampler;

            /* Skip animations without a target node. Consistent with
               tinygltf's behavior, currently there are no extensions for
               animating materials or anything else so there's no point in
               importing such animations. */
            /** @todo revisit once KHR_animation2 is a thing:
                https://github.com/KhronosGroup/glTF/pull/2033 */
            if(!channel.target_node)
                continue;

            /* Key properties -- always float time. Not using checkAccessor()
               as this was all checked above once already. */
            const cgltf_accessor* input = sampler.input;
            if(input->type != cgltf_type_scalar || input->component_type != cgltf_component_type_r_32f || input->normalized) {
                Error{} << "Trade::CgltfImporter::animation(): time track has unexpected type"
                    << (input->normalized ? "normalized " : "") << Debug::nospace
                    << gltfTypeName(input->type) << "/" << gltfComponentTypeName(input->component_type);
                return Containers::NullOpt;
            }

            /* View on the key data */
            const auto inputDataFound = samplerData.find(input);
            CORRADE_INTERNAL_ASSERT(inputDataFound != samplerData.end());
            const auto keys = Containers::arrayCast<Float>(
                data.suffix(inputDataFound->second.outputOffset).prefix(
                    inputDataFound->second.src.size()[0]*
                    inputDataFound->second.src.size()[1]));

            /* Interpolation mode */
            Animation::Interpolation interpolation;
            if(sampler.interpolation == cgltf_interpolation_type_linear) {
                interpolation = Animation::Interpolation::Linear;
            } else if(sampler.interpolation == cgltf_interpolation_type_cubic_spline) {
                interpolation = Animation::Interpolation::Spline;
            } else if(sampler.interpolation == cgltf_interpolation_type_step) {
                interpolation = Animation::Interpolation::Constant;
            } else {
                /* There is no cgltf_interpolation_type_invalid, cgltf falls
                   back to linear for invalid interpolation modes */
                CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            }

            /* Decide on value properties. Not using checkAccessor() as this
               was all checked above once already. */
            const cgltf_accessor* output = sampler.output;
            AnimationTrackTargetType target;
            AnimationTrackType type, resultType;
            Animation::TrackViewStorage<const Float> track;
            const auto outputDataFound = samplerData.find(output);
            CORRADE_INTERNAL_ASSERT(outputDataFound != samplerData.end());
            const auto outputData = data.suffix(outputDataFound->second.outputOffset)
                .prefix(outputDataFound->second.src.size()[0]*
                        outputDataFound->second.src.size()[1]);
            const cgltf_accessor*& timeTrackUsed = outputDataFound->second.timeTrack;

            const std::size_t valuesPerKey = interpolation == Animation::Interpolation::Spline ? 3 : 1;
            if(input->count*valuesPerKey != output->count) {
                Error{} << "Trade::CgltfImporter::animation(): target track size doesn't match time track size, expected" << output->count << "but got" << input->count*valuesPerKey;
                return Containers::NullOpt;
            }

            /* Translation */
            if(channel.target_path == cgltf_animation_path_type_translation) {
                if(output->type != cgltf_type_vec3 || output->component_type != cgltf_component_type_r_32f || output->normalized) {
                    Error{} << "Trade::CgltfImporter::animation(): translation track has unexpected type"
                        << (output->normalized ? "normalized " : "") << Debug::nospace
                        << gltfTypeName(output->type) << "/" << gltfComponentTypeName(output->component_type);
                    return Containers::NullOpt;
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Translation3D;
                resultType = AnimationTrackType::Vector3;
                if(interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once
                       for every track -- postprocessSplineTrack() checks
                       that. */
                    const auto values = Containers::arrayCast<CubicHermite3D>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermite3D;
                    track = Animation::TrackView<const Float, const CubicHermite3D>{
                        keys, values, interpolation,
                        animationInterpolatorFor<CubicHermite3D>(interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    type = AnimationTrackType::Vector3;
                    track = Animation::TrackView<const Float, const Vector3>{keys,
                        Containers::arrayCast<Vector3>(outputData),
                        interpolation,
                        animationInterpolatorFor<Vector3>(interpolation),
                        Animation::Extrapolation::Constant};
                }

            /* Rotation */
            } else if(channel.target_path == cgltf_animation_path_type_rotation) {
                /** @todo rotation can be also normalized (?!) to a vector of 8/16bit (signed?!) integers
                    cgltf_accessor_unpack_floats might help with unpacking them */

                if(output->type != cgltf_type_vec4 || output->component_type != cgltf_component_type_r_32f || output->normalized) {
                    Error{} << "Trade::CgltfImporter::animation(): rotation track has unexpected type"
                        << (output->normalized ? "normalized " : "") << Debug::nospace
                        << gltfTypeName(output->type) << "/" << gltfComponentTypeName(output->component_type);
                    return Containers::NullOpt;
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Rotation3D;
                resultType = AnimationTrackType::Quaternion;
                if(interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once
                       for every track -- postprocessSplineTrack() checks
                       that. */
                    const auto values = Containers::arrayCast<CubicHermiteQuaternion>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermiteQuaternion;
                    track = Animation::TrackView<const Float, const CubicHermiteQuaternion>{
                        keys, values, interpolation,
                        animationInterpolatorFor<CubicHermiteQuaternion>(interpolation),
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
                        keys, values, interpolation,
                        animationInterpolatorFor<Quaternion>(interpolation),
                        Animation::Extrapolation::Constant};
                }

            /* Scale */
            } else if(channel.target_path == cgltf_animation_path_type_scale) {
                if(output->type != cgltf_type_vec3 || output->component_type != cgltf_component_type_r_32f || output->normalized) {
                    Error{} << "Trade::CgltfImporter::animation(): scaling track has unexpected type"
                        << (output->normalized ? "normalized " : "") << Debug::nospace
                        << gltfTypeName(output->type) << "/" << gltfComponentTypeName(output->component_type);
                    return Containers::NullOpt;
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Scaling3D;
                resultType = AnimationTrackType::Vector3;
                if(interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once
                       for every track -- postprocessSplineTrack() checks
                       that. */
                    const auto values = Containers::arrayCast<CubicHermite3D>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermite3D;
                    track = Animation::TrackView<const Float, const CubicHermite3D>{
                        keys, values, interpolation,
                        animationInterpolatorFor<CubicHermite3D>(interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    type = AnimationTrackType::Vector3;
                    track = Animation::TrackView<const Float, const Vector3>{keys,
                        Containers::arrayCast<Vector3>(outputData),
                        interpolation,
                        animationInterpolatorFor<Vector3>(interpolation),
                        Animation::Extrapolation::Constant};
                }

            } else {
                Error{} << "Trade::CgltfImporter::animation(): unsupported track target" << channel.target_path;
                return Containers::NullOpt;
            }

            /* Splines were postprocessed using the corresponding time track.
               If a spline is not yet marked as postprocessed, mark it.
               Otherwise check that the spline track is always used with the
               same time track. */
            if(interpolation == Animation::Interpolation::Spline) {
                if(timeTrackUsed == nullptr)
                    timeTrackUsed = sampler.input;
                else if(timeTrackUsed != sampler.input) {
                    Error{} << "Trade::CgltfImporter::animation(): spline track is shared with different time tracks, we don't support that, sorry";
                    return Containers::NullOpt;
                }
            }

            const UnsignedInt targetId = channel.target_node - _d->data->nodes;
            tracks[trackId++] = AnimationTrackData{type, resultType, target,
                /* In cases where multi-primitive mesh nodes are split into
                   multiple objects, the animation should affect the first node
                   -- the other nodes are direct children of it and so they get
                   affected too */
                UnsignedInt(_d->nodeSizeOffsets[targetId]),
                track};
        }
    }

    if(hadToRenormalize)
        Warning{} << "Trade::CgltfImporter::animation(): quaternions in some rotation tracks were renormalized";

    return AnimationData{std::move(data), std::move(tracks)};
}

UnsignedInt CgltfImporter::doCameraCount() const {
    return _d->data->cameras_count;
}

Int CgltfImporter::doCameraForName(const std::string& name) {
    if(!_d->camerasForName) {
        _d->camerasForName.emplace();
        _d->camerasForName->reserve(_d->data->cameras_count);
        for(std::size_t i = 0; i != _d->data->cameras_count; ++i)
            _d->camerasForName->emplace(_d->decodeString(_d->data->cameras[i].name), i);
    }

    const auto found = _d->camerasForName->find(name);
    return found == _d->camerasForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doCameraName(const UnsignedInt id) {
    return _d->decodeString(_d->data->cameras[id].name);
}

Containers::Optional<CameraData> CgltfImporter::doCamera(UnsignedInt id) {
    const cgltf_camera& camera = _d->data->cameras[id];

    /* https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#projection-matrices */

    /* Perspective camera. glTF uses vertical FoV and X/Y aspect ratio, so to
       avoid accidental bugs we will directly calculate the near plane size and
       use that to create the camera data (instead of passing it the horizontal
       FoV). */
    /** @todo What if znear == 0? aspect_ratio == 0? cgltf exposes
        has_aspect_ratio */
    if(camera.type == cgltf_camera_type_perspective) {
        const cgltf_camera_perspective& data = camera.data.perspective;
        const Vector2 size = 2.0f*data.znear*Math::tan(data.yfov*0.5_radf)*Vector2::xScale(data.aspect_ratio);
        const Float far = data.has_zfar ? data.zfar : Constants::inf();
        return CameraData{CameraType::Perspective3D, size, data.znear, far};
    }

    /* Orthographic camera. glTF uses a "scale" instead of "size", which means
       we have to double. */
    if(camera.type == cgltf_camera_type_orthographic) {
        const cgltf_camera_orthographic& data = camera.data.orthographic;
        return CameraData{CameraType::Orthographic3D,
            Vector2{data.xmag, data.ymag}*2.0f, data.znear, data.zfar};
    }

    CORRADE_INTERNAL_ASSERT(camera.type == cgltf_camera_type_invalid);
    Error{} << "Trade::CgltfImporter::camera(): invalid camera type";
    return Containers::NullOpt;
}

UnsignedInt CgltfImporter::doLightCount() const {
    return _d->data->lights_count;
}

Int CgltfImporter::doLightForName(const std::string& name) {
    if(!_d->lightsForName) {
        _d->lightsForName.emplace();
        _d->lightsForName->reserve(_d->data->lights_count);
        for(std::size_t i = 0; i != _d->data->lights_count; ++i)
            _d->lightsForName->emplace(_d->decodeString(_d->data->lights[i].name), i);
    }

    const auto found = _d->lightsForName->find(name);
    return found == _d->lightsForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doLightName(const UnsignedInt id) {
    return _d->decodeString(_d->data->lights[id].name);
}

Containers::Optional<LightData> CgltfImporter::doLight(UnsignedInt id) {
    const cgltf_light& light = _d->data->lights[id];

    /* https://github.com/KhronosGroup/glTF/tree/5d3dfa44e750f57995ac6821117d9c7061bba1c9/extensions/2.0/Khronos/KHR_lights_punctual */

    /* Light type */
    LightData::Type type;
    if(light.type == cgltf_light_type_point) {
        type = LightData::Type::Point;
    } else if(light.type == cgltf_light_type_spot) {
        type = LightData::Type::Spot;
    } else if(light.type == cgltf_light_type_directional) {
        type = LightData::Type::Directional;
    } else {
        CORRADE_INTERNAL_ASSERT(light.type == cgltf_light_type_invalid);
        Error{} << "Trade::CgltfImporter::light(): invalid light type";
        return Containers::NullOpt;
    }

    /* Cgltf sets range to 0 instead of infinity when it's not present.
       That's stupid because it would divide by zero, fix that. Even more
       stupid is JSON not having ANY way to represent an infinity, FFS. */
    const Float range = light.range == 0.0f ? Constants::inf() : light.range;

    /* Spotlight cone angles. In glTF they're specified as half-angles (which
       is also why the limit on outer angle is 90°, not 180°), to avoid
       confusion report a potential error in the original half-angles and
       double the angle only at the end. */
    Rad innerConeAngle{NoInit}, outerConeAngle{NoInit};
    if(type == LightData::Type::Spot) {
        innerConeAngle = Rad{light.spot_inner_cone_angle};
        outerConeAngle = Rad{light.spot_outer_cone_angle};

        if(innerConeAngle < Rad(0.0_degf) || innerConeAngle >= outerConeAngle || outerConeAngle >= Rad(90.0_degf)) {
            Error{} << "Trade::CgltfImporter::light(): inner and outer cone angle" << Deg(innerConeAngle) << "and" << Deg(outerConeAngle) << "out of allowed bounds";
            return Containers::NullOpt;
        }
    } else innerConeAngle = outerConeAngle = 180.0_degf;

    /* Range should be infinity for directional lights. Because there's no way
       to represent infinity in JSON, directly suggest to remove the range
       property, don't even bother printing the value. */
    if(type == LightData::Type::Directional && range != Constants::inf()) {
        Error{} << "Trade::CgltfImporter::light(): range can't be defined for a directional light";
        return Containers::NullOpt;
    }

    /* As said above, glTF uses half-angles, while we have full angles (for
       consistency with existing APIs such as OpenAL cone angles or math intersection routines as well as Blender). */
    return LightData{type, Color3::from(light.color), light.intensity, range, innerConeAngle*2.0f, outerConeAngle*2.0f};
}

Int CgltfImporter::doDefaultScene() const {
    /* While https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#scenes
       says that "When scene is undefined, client implementations MAY delay
       rendering until a particular scene is requested.", several official
       sample glTF models (e.g. the AnimatedTriangle) have no "scene" property,
       so that's a bit stupid behavior to have. As per discussion at
       https://github.com/KhronosGroup/glTF/issues/815#issuecomment-274286889,
       if a default scene isn't defined and there is at least one scene, just
       use the first one. */
    if(!_d->data->scene)
        return _d->data->scenes_count > 0 ? 0 : -1;

    const Int sceneId = _d->data->scene - _d->data->scenes;
    return sceneId;
}

UnsignedInt CgltfImporter::doSceneCount() const {
    return _d->data->scenes_count;
}

Int CgltfImporter::doSceneForName(const std::string& name) {
    if(!_d->scenesForName) {
        _d->scenesForName.emplace();
        _d->scenesForName->reserve(_d->data->scenes_count);
        for(std::size_t i = 0; i != _d->data->scenes_count; ++i)
            _d->scenesForName->emplace(_d->decodeString(_d->data->scenes[i].name), i);
    }

    const auto found = _d->scenesForName->find(name);
    return found == _d->scenesForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doSceneName(const UnsignedInt id) {
    return _d->decodeString(_d->data->scenes[id].name);
}

Containers::Optional<SceneData> CgltfImporter::doScene(UnsignedInt id) {
    const cgltf_scene& scene = _d->data->scenes[id];

    /* Cgltf checks all node / mesh / light ... references during initial file
       parsing (and unfortunately produces a rather unhelpful generic message)
       so this code doesn't have to do any range checks anymore. */

    /* Gather all top-level nodes belonging to a scene and recursively populate
       the children ranges. Optimistically assume the glTF has just a single
       scene and reserve for that. */
    /** @todo once we have BitArrays use the objects array to mark nodes that
        are present in the scene and then create a new array from those but
        ordered so we can have OrderedMapping for parents and also all other
        fields */
    Containers::Array<UnsignedInt> objects;
    arrayReserve(objects, _d->data->nodes_count);
    for(UnsignedInt i = 0; i != scene.nodes_count; ++i) {
        arrayAppend(objects, scene.nodes[i] - _d->data->nodes);
    }

    Containers::Array<Range1Dui> children;
    arrayReserve(children, _d->data->nodes_count + 1);
    arrayAppend(children, InPlaceInit, 0u, UnsignedInt(objects.size()));
    for(std::size_t i = 0; i != children.size(); ++i) {
        const Range1Dui& nodeRangeToProcess = children[i];
        for(std::size_t j = nodeRangeToProcess.min(); j != nodeRangeToProcess.max(); ++j) {
            arrayAppend(children, InPlaceInit, UnsignedInt(objects.size()), UnsignedInt(objects.size() + _d->data->nodes[objects[j]].children_count));
            const cgltf_node& node = _d->data->nodes[objects[j]];
            for(std::size_t k = 0; k != node.children_count; ++k)
                arrayAppend(objects, UnsignedInt(node.children[k] - _d->data->nodes));
        }
    }

    /** @todo once there's SceneData::mappingRange(), calculate also min here */
    const UnsignedInt maxObjectIndex = Math::max(objects);

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
        const cgltf_node& node = _d->data->nodes[i];

        /* Everything that has a TRS should have a transformation matrix as
           well. OTOH there can be a transformation matrix but no TRS, and
           there can also be objects without any transformation. */
        if(node.has_translation ||
           node.has_rotation ||
           node.has_scale) {
            ++trsCount;
            ++transformationCount;
        } else if(node.has_matrix) ++transformationCount;

        if(node.has_translation) hasTranslations = true;
        if(node.has_rotation) hasRotations = true;
        if(node.has_scale) hasScalings = true;
        if(node.mesh) {
            meshCount += node.mesh->primitives_count;
            for(std::size_t j = 0; j != node.mesh->primitives_count; ++j) {
                if(node.mesh->primitives[j].material) {
                    hasMeshMaterials = true;
                    break;
                }
            }
        }
        if(node.camera) ++cameraCount;
        if(node.skin) ++skinCount;
        if(node.light) ++lightCount;
    }

    /* If all objects that have transformations have TRS as well, no need to
       store the combined transform field */
    if(trsCount == transformationCount) transformationCount = 0;

    /* Allocate the output array */
    Containers::ArrayView<UnsignedInt> parentObjects;
    Containers::ArrayView<Int> parents;
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
        {NoInit, objects.size(), parentObjects},
        {NoInit, objects.size(), parents},
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
    Utility::copy(objects, parentObjects);
    for(std::size_t i = 0; i != children.size(); ++i) {
        Int parent = Int(i) - 1;
        for(std::size_t j = children[i].min(); j != children[i].max(); ++j)
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
        const cgltf_node& node = _d->data->nodes[objects[i]];

        /* Parse TRS */
        Vector3 translation;
        if(node.has_translation) translation = Vector3::from(node.translation);

        Quaternion rotation;
        if(node.has_rotation) {
            rotation = Quaternion{Vector3::from(node.rotation), node.rotation[3]};
            if(!rotation.isNormalized() && configuration().value<bool>("normalizeQuaternions")) {
                rotation = rotation.normalized();
                Warning{} << "Trade::CgltfImporter::scene(): rotation quaternion of node" << objects[i] << "was renormalized";
            }
        }

        Vector3 scaling{1.0f};
        if(node.has_scale) scaling = Vector3::from(node.scale);

        /* Parse transformation, or combine it from TRS if not present */
        Matrix4 transformation;
        if(node.has_matrix) transformation = Matrix4::from(node.matrix);
        else transformation =
            Matrix4::translation(translation)*
            Matrix4{rotation.toMatrix()}*
            Matrix4::scaling(scaling);

        /* Populate the combined transformation and object mapping only if
           there's actually some transformation for this object and we want to
           store it -- if all objects have TRS anyway, the matrix is redundant */
        if((node.has_matrix ||
            node.has_translation ||
            node.has_rotation ||
            node.has_scale) && transformationCount)
        {
            transformations[transformationOffset] = transformation;
            transformationObjects[transformationOffset] = objects[i];
            ++transformationOffset;
        }

        /* Store the TRS information and object mapping only if there was
           something */
        if(node.has_translation ||
           node.has_rotation ||
           node.has_scale)
        {
            if(hasTranslations) translations[trsOffset] = translation;
            if(hasRotations) rotations[trsOffset] = rotation;
            if(hasScalings) scalings[trsOffset] = scaling;
            trsObjects[trsOffset] = objects[i];
            ++trsOffset;
        }

        /* Populate mesh references */
        if(node.mesh) {
            for(std::size_t j = 0; j != node.mesh->primitives_count; ++j) {
                meshMaterialObjects[meshMaterialOffset] = objects[i];
                meshes[meshMaterialOffset] = _d->meshSizeOffsets[node.mesh - _d->data->meshes] + j;
                if(hasMeshMaterials) {
                    const cgltf_material* material = node.mesh->primitives[j].material;
                    meshMaterials[meshMaterialOffset] = material ? material - _d->data->materials : -1;
                }
                ++meshMaterialOffset;
            }
        }

        /* Populate light references */
        if(node.light) {
            lightObjects[lightOffset] = objects[i];
            lights[lightOffset] = node.light - _d->data->lights;
            ++lightOffset;
        }

        /* Populate camera references */
        if(node.camera) {
            cameraObjects[cameraOffset] = objects[i];
            cameras[cameraOffset] = node.camera - _d->data->cameras;
            ++cameraOffset;
        }

        /* Populate skin references */
        if(node.skin) {
            skinObjects[skinOffset] = objects[i];
            skins[skinOffset] = node.skin - _d->data->skins;
            ++skinOffset;
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
        SceneFieldData{SceneField::Parent, parentObjects, parents}
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
    return SceneData{SceneMappingType::UnsignedInt, maxObjectIndex + 1, std::move(data), std::move(fields)};
}

UnsignedLong CgltfImporter::doObjectCount() const {
    return _d->data->nodes_count;
}

Long CgltfImporter::doObjectForName(const std::string& name) {
    if(!_d->nodesForName) {
        _d->nodesForName.emplace();
        _d->nodesForName->reserve(_d->data->nodes_count);
        for(std::size_t i = 0; i != _d->data->nodes_count; ++i) {
            _d->nodesForName->emplace(_d->decodeString(_d->data->nodes[i].name), i);
        }
    }

    const auto found = _d->nodesForName->find(name);
    return found == _d->nodesForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doObjectName(UnsignedLong id) {
    return _d->decodeString(_d->data->nodes[id].name);
}

UnsignedInt CgltfImporter::doSkin3DCount() const {
    return _d->data->skins_count;
}

Int CgltfImporter::doSkin3DForName(const std::string& name) {
    if(!_d->skinsForName) {
        _d->skinsForName.emplace();
        _d->skinsForName->reserve(_d->data->skins_count);
        for(std::size_t i = 0; i != _d->data->skins_count; ++i)
            _d->skinsForName->emplace(_d->decodeString(_d->data->skins[i].name), i);
    }

    const auto found = _d->skinsForName->find(name);
    return found == _d->skinsForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doSkin3DName(const UnsignedInt id) {
    return _d->decodeString(_d->data->skins[id].name);
}

Containers::Optional<SkinData3D> CgltfImporter::doSkin3D(const UnsignedInt id) {
    const cgltf_skin& skin = _d->data->skins[id];

    if(!skin.joints_count) {
        Error{} << "Trade::CgltfImporter::skin3D(): skin has no joints";
        return Containers::NullOpt;
    }

    /* Joint IDs */
    Containers::Array<UnsignedInt> joints{NoInit, skin.joints_count};
    for(std::size_t i = 0; i != joints.size(); ++i) {
        const UnsignedInt nodeId = skin.joints[i] - _d->data->nodes;
        joints[i] = nodeId;
    }

    /* Inverse bind matrices. If there are none, default is identities */
    Containers::Array<Matrix4> inverseBindMatrices{skin.joints_count};
    if(skin.inverse_bind_matrices) {
        const cgltf_accessor* accessor = skin.inverse_bind_matrices;
        if(!checkAccessor(_d->data, "skin3D", accessor))
            return Containers::NullOpt;

        if(accessor->type != cgltf_type_mat4 || accessor->component_type != cgltf_component_type_r_32f || accessor->normalized) {
            Error{} << "Trade::CgltfImporter::skin3D(): inverse bind matrices have unexpected type"
                << (accessor->normalized ? "normalized " : "") << Debug::nospace
                << gltfTypeName(accessor->type) << "/" << gltfComponentTypeName(accessor->component_type);
            return Containers::NullOpt;
        }

        Containers::Optional<Containers::StridedArrayView2D<const char>> view = _d->accessorView(accessor, "skin3D");
        if(!view)
            return Containers::NullOpt;

        Containers::StridedArrayView1D<const Matrix4> matrices = Containers::arrayCast<1, const Matrix4>(*view);
        if(matrices.size() != inverseBindMatrices.size()) {
            Error{} << "Trade::CgltfImporter::skin3D(): invalid inverse bind matrix count, expected" << inverseBindMatrices.size() << "but got" << matrices.size();
            return Containers::NullOpt;
        }

        Utility::copy(matrices, inverseBindMatrices);
    }

    return SkinData3D{std::move(joints), std::move(inverseBindMatrices)};
}

UnsignedInt CgltfImporter::doMeshCount() const {
    return _d->meshMap.size();
}

Int CgltfImporter::doMeshForName(const std::string& name) {
    if(!_d->meshesForName) {
        _d->meshesForName.emplace();
        _d->meshesForName->reserve(_d->data->meshes_count);
        for(std::size_t i = 0; i != _d->data->meshes_count; ++i) {
            /* The mesh can be duplicated for as many primitives as it has,
               point to the first mesh in the duplicate sequence */
            _d->meshesForName->emplace(_d->decodeString(_d->data->meshes[i].name), _d->meshSizeOffsets[i]);
        }
    }

    const auto found = _d->meshesForName->find(name);
    return found == _d->meshesForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doMeshName(const UnsignedInt id) {
    /* This returns the same name for all multi-primitive mesh duplicates */
    return _d->decodeString(_d->data->meshes[_d->meshMap[id].first()].name);
}

Containers::Optional<MeshData> CgltfImporter::doMesh(const UnsignedInt id, UnsignedInt) {
    const cgltf_mesh& mesh = _d->data->meshes[_d->meshMap[id].first()];
    const cgltf_primitive& primitive = mesh.primitives[_d->meshMap[id].second()];

    MeshPrimitive meshPrimitive{};
    if(primitive.type == cgltf_primitive_type_points) {
        meshPrimitive = MeshPrimitive::Points;
    } else if(primitive.type == cgltf_primitive_type_lines) {
        meshPrimitive = MeshPrimitive::Lines;
    } else if(primitive.type == cgltf_primitive_type_line_loop) {
        meshPrimitive = MeshPrimitive::LineLoop;
    } else if(primitive.type == cgltf_primitive_type_line_strip) {
        meshPrimitive = MeshPrimitive::LineStrip;
    } else if(primitive.type == cgltf_primitive_type_triangles) {
        meshPrimitive = MeshPrimitive::Triangles;
    } else if(primitive.type == cgltf_primitive_type_triangle_fan) {
        meshPrimitive = MeshPrimitive::TriangleFan;
    } else if(primitive.type == cgltf_primitive_type_triangle_strip) {
        meshPrimitive = MeshPrimitive::TriangleStrip;
    } else {
        /* Cgltf parses an int and directly casts it to cgltf_primitive_type
           without checking for valid values */
        Error{} << "Trade::CgltfImporter::mesh(): unrecognized primitive" << primitive.type;
        return Containers::NullOpt;
    }

    /* Sort attributes by name so that we add attribute sets in the correct
       order and can warn if indices are not contiguous. Stable sort is needed
       to preserve declaration order for duplicate attributes, checked below. */
    Containers::Array<UnsignedInt> attributeOrder{primitive.attributes_count};
    for(UnsignedInt i = 0; i < attributeOrder.size(); ++i)
        attributeOrder[i] = i;

    std::stable_sort(attributeOrder.begin(), attributeOrder.end(), [&](UnsignedInt a, UnsignedInt b) {
            return std::strcmp(primitive.attributes[a].name, primitive.attributes[b].name) < 0;
        });

    /* Find and remove duplicate attributes. This mimics tinygltf behaviour
       which replaces the previous attribute of the same name. */
    std::size_t attributeCount = attributeOrder.size();
    for(UnsignedInt i = 0; i + 1 < attributeOrder.size(); ++i) {
        const cgltf_attribute& current = primitive.attributes[attributeOrder[i]];
        const cgltf_attribute& next = primitive.attributes[attributeOrder[i + 1]];
        if(std::strcmp(current.name, next.name) == 0) {
            --attributeCount;
            /* Mark for skipping later */
            attributeOrder[i] = ~0u;
        }
    }

    /* Gather all (whitelisted) attributes and the total buffer range spanning
       them */
    cgltf_buffer* buffer = nullptr;
    UnsignedInt vertexCount = 0;
    std::size_t attributeId = 0;
    cgltf_attribute lastAttribute{};
    Math::Range1D<std::size_t> bufferRange;
    Containers::Array<MeshAttributeData> attributeData{attributeCount};
    for(UnsignedInt a: attributeOrder) {
        /* Duplicate attribute, skip */
        if(a == ~0u)
            continue;

        const cgltf_attribute& attribute = primitive.attributes[a];

        const Containers::StringView nameString{attribute.name};
        /* See the comment in doOpenData() for why we do this */
        const Containers::StringView semantic = attribute.type != cgltf_attribute_type_invalid ?
            nameString.partition('_')[0] : nameString;

        /* Numbered attributes are expected to be contiguous (COLORS_0,
           COLORS_1...). If not, print a warning, because in the MeshData they
           will appear as contiguous. */
        if(attribute.type != cgltf_attribute_type_invalid) {
            if(attribute.type != lastAttribute.type)
                lastAttribute.index = -1;

            if(attribute.index != lastAttribute.index + 1)
                Warning{} << "Trade::CgltfImporter::mesh(): found attribute" << nameString << "but expected" << semantic << Debug::nospace << "_" << Debug::nospace << lastAttribute.index + 1;
        }
        lastAttribute = attribute;

        const cgltf_accessor* accessor = attribute.data;
        if(!checkAccessor(_d->data, "mesh", accessor))
            return Containers::NullOpt;

        /* Convert to our vertex format */
        VertexFormat componentFormat;
        if(accessor->component_type == cgltf_component_type_r_8)
            componentFormat = VertexFormat::Byte;
        else if(accessor->component_type == cgltf_component_type_r_8u)
            componentFormat = VertexFormat::UnsignedByte;
        else if(accessor->component_type == cgltf_component_type_r_16)
            componentFormat = VertexFormat::Short;
        else if(accessor->component_type == cgltf_component_type_r_16u)
            componentFormat = VertexFormat::UnsignedShort;
        else if(accessor->component_type == cgltf_component_type_r_32u)
            componentFormat = VertexFormat::UnsignedInt;
        else if(accessor->component_type == cgltf_component_type_r_32f)
            componentFormat = VertexFormat::Float;
        else {
            CORRADE_INTERNAL_ASSERT(accessor->component_type == cgltf_component_type_invalid);
            Error{} << "Trade::CgltfImporter::mesh(): attribute" << nameString << "has an invalid component type";
            return {};
        }

        UnsignedInt componentCount;
        UnsignedInt vectorCount = 0;
        if(accessor->type == cgltf_type_scalar)
            componentCount = 1;
        else if(accessor->type == cgltf_type_vec2)
            componentCount = 2;
        else if(accessor->type == cgltf_type_vec3)
            componentCount = 3;
        else if(accessor->type == cgltf_type_vec4)
            componentCount = 4;
        else if(accessor->type == cgltf_type_mat2) {
            componentCount = 2;
            vectorCount = 2;
        } else if(accessor->type == cgltf_type_mat3) {
            componentCount = 3;
            vectorCount = 3;
        } else if(accessor->type == cgltf_type_mat4) {
            componentCount = 4;
            vectorCount = 4;
        } else {
            CORRADE_INTERNAL_ASSERT(accessor->type == cgltf_type_invalid);
            Error{} << "Trade::CgltfImporter::mesh(): attribute" << nameString << "has an invalid type";
            return {};
        }

        /* Check for illegal normalized types */
        if(accessor->normalized &&
            (componentFormat == VertexFormat::Float || componentFormat == VertexFormat::UnsignedInt)) {
            Error{} << "Trade::CgltfImporter::mesh(): attribute" << nameString << "component type" << gltfComponentTypeName(accessor->component_type) << "can't be normalized";
            return Containers::NullOpt;
        }

        /* Check that matrix type is legal */
        if(vectorCount &&
            componentFormat != VertexFormat::Float &&
            !(componentFormat == VertexFormat::Byte && accessor->normalized) &&
            !(componentFormat == VertexFormat::Short && accessor->normalized)) {
            Error{} << "Trade::CgltfImporter::mesh(): attribute" << nameString << "has an unsupported matrix component type"
                << (accessor->normalized ? "normalized" : "unnormalized")
                << gltfComponentTypeName(accessor->component_type);
            return Containers::NullOpt;
        }

        const VertexFormat format = vectorCount ?
            vertexFormat(componentFormat, vectorCount, componentCount, true) :
            vertexFormat(componentFormat, componentCount, accessor->normalized);

        /* Whitelist supported attribute and data type combinations */
        MeshAttribute name;
        if(attribute.type == cgltf_attribute_type_position) {
            name = MeshAttribute::Position;

            if(accessor->type != cgltf_type_vec3) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected" << semantic << "type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            if(!(componentFormat == VertexFormat::Float && !accessor->normalized) &&
               /* KHR_mesh_quantization. Both normalized and unnormalized
                  bytes/shorts are okay. */
               componentFormat != VertexFormat::UnsignedByte &&
               componentFormat != VertexFormat::Byte &&
               componentFormat != VertexFormat::UnsignedShort &&
               componentFormat != VertexFormat::Short) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported" << semantic << "component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }

        } else if(attribute.type == cgltf_attribute_type_normal) {
            name = MeshAttribute::Normal;

            if(accessor->type != cgltf_type_vec3) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected" << semantic << "type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            if(!(componentFormat == VertexFormat::Float && !accessor->normalized) &&
               /* KHR_mesh_quantization */
               !(componentFormat == VertexFormat::Byte && accessor->normalized) &&
               !(componentFormat == VertexFormat::Short && accessor->normalized)) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported" << semantic << "component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }

        } else if(attribute.type == cgltf_attribute_type_tangent) {
            name = MeshAttribute::Tangent;

            if(accessor->type != cgltf_type_vec4) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected" << semantic << "type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            if(!(componentFormat == VertexFormat::Float && !accessor->normalized) &&
               /* KHR_mesh_quantization */
               !(componentFormat == VertexFormat::Byte && accessor->normalized) &&
               !(componentFormat == VertexFormat::Short && accessor->normalized)) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported" << semantic << "component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }

        } else if(attribute.type == cgltf_attribute_type_texcoord) {
            name = MeshAttribute::TextureCoordinates;

            if(accessor->type != cgltf_type_vec2) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected" << semantic << "type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            /* Core spec only allows float and normalized unsigned bytes/shorts, the
               rest is added by KHR_mesh_quantization */
            if(!(componentFormat == VertexFormat::Float && !accessor->normalized) &&
               componentFormat != VertexFormat::UnsignedByte &&
               componentFormat != VertexFormat::Byte &&
               componentFormat != VertexFormat::UnsignedShort &&
               componentFormat != VertexFormat::Short) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported" << semantic << "component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }

        } else if(attribute.type == cgltf_attribute_type_color) {
            name = MeshAttribute::Color;

            if(accessor->type != cgltf_type_vec4 && accessor->type != cgltf_type_vec3) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected" << semantic << "type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            if(!(componentFormat == VertexFormat::Float && !accessor->normalized) &&
               !(componentFormat == VertexFormat::UnsignedByte && accessor->normalized) &&
               !(componentFormat == VertexFormat::UnsignedShort && accessor->normalized)) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported" << semantic << "component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }
        } else if(attribute.type == cgltf_attribute_type_joints) {
            name = _d->meshAttributesForName.at(semantic);

            if(accessor->type != cgltf_type_vec4) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected" << semantic << "type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            if(!(componentFormat == VertexFormat::UnsignedByte && !accessor->normalized) &&
               !(componentFormat == VertexFormat::UnsignedShort && !accessor->normalized)) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported" << semantic << "component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }
        } else if(attribute.type == cgltf_attribute_type_weights) {
            name = _d->meshAttributesForName.at(semantic);

            if(accessor->type != cgltf_type_vec4) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected" << semantic << "type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            if(!(componentFormat == VertexFormat::Float && !accessor->normalized) &&
               !(componentFormat == VertexFormat::UnsignedByte && accessor->normalized) &&
               !(componentFormat == VertexFormat::UnsignedShort && accessor->normalized)) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported" << semantic << "component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }
        /* Object ID, name user-configurable */
        } else if(nameString == configuration().value("objectIdAttribute")) {
            name = MeshAttribute::ObjectId;

            if(accessor->type != cgltf_type_scalar) {
                Error{} << "Trade::CgltfImporter::mesh(): unexpected object ID type" << gltfTypeName(accessor->type);
                return Containers::NullOpt;
            }

            /* The glTF spec says that "Application-specific attribute semantics
               MUST NOT use unsigned int component type" but I'm not sure what
               the point of enforcing that would be */
            if((componentFormat != VertexFormat::UnsignedInt &&
                componentFormat != VertexFormat::UnsignedShort &&
                componentFormat != VertexFormat::UnsignedByte) ||
                accessor->normalized) {
                Error{} << "Trade::CgltfImporter::mesh(): unsupported object ID component type"
                    << (accessor->normalized ? "normalized" : "unnormalized")
                    << gltfComponentTypeName(accessor->component_type);
                return Containers::NullOpt;
            }

        /* Custom or unrecognized attributes, map to an ID */
        } else {
            CORRADE_INTERNAL_ASSERT(attribute.type == cgltf_attribute_type_invalid);
            name = _d->meshAttributesForName.at(nameString);
        }

        /* Remember which buffer the attribute is in and the range, for
           consecutive attribs expand the range */
        const cgltf_buffer_view* bufferView = accessor->buffer_view;
        if(attributeId == 0) {
            buffer = bufferView->buffer;
            bufferRange = Math::Range1D<std::size_t>::fromSize(bufferView->offset, bufferView->size);
            vertexCount = accessor->count;
        } else {
            /* ... and probably never will be */
            if(bufferView->buffer != buffer) {
                Error{} << "Trade::CgltfImporter::mesh(): meshes spanning multiple buffers are not supported";
                return Containers::NullOpt;
            }

            bufferRange = Math::join(bufferRange, Math::Range1D<std::size_t>::fromSize(bufferView->offset, bufferView->size));

            if(accessor->count != vertexCount) {
                Error{} << "Trade::CgltfImporter::mesh(): mismatched vertex count for attribute" << semantic << Debug::nospace << ", expected" << vertexCount << "but got" << accessor->count;
                return Containers::NullOpt;
            }
        }

        /** @todo Check that accessor stride >= vertexFormatSize(format)? */

        /* Fill in an attribute. Offset-only, will be patched to be relative to
           the actual output buffer once we know how large it is and where it
           is allocated. */
        attributeData[attributeId++] = MeshAttributeData{name, format,
            UnsignedInt(accessor->offset + bufferView->offset), vertexCount,
            std::ptrdiff_t(accessor->stride)};
    }

    /* Verify we really filled all attributes */
    CORRADE_INTERNAL_ASSERT(attributeId == attributeData.size());

    /* Allocate & copy vertex data (if any) */
    Containers::Array<char> vertexData{NoInit, bufferRange.size()};
    if(vertexData.size()) {
        const UnsignedInt bufferId = buffer - _d->data->buffers;
        if(!_d->loadBuffer(bufferId, "mesh"))
            return {};

        Utility::copy(Containers::arrayView(static_cast<char*>(buffer->data), buffer->size)
            .slice(bufferRange.min(), bufferRange.max()),
        vertexData);
    }

    /* Convert the attributes from relative to absolute, copy them to a
       non-growable array and do additional patching */
    for(std::size_t i = 0; i != attributeData.size(); ++i) {
        Containers::StridedArrayView1D<char> data{vertexData,
            /* Offset is what with the range min subtracted, as we copied
               without the prefix */
            vertexData + attributeData[i].offset(vertexData) - bufferRange.min(),
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
    if(primitive.indices) {
        const cgltf_accessor* accessor = primitive.indices;
        if(!checkAccessor(_d->data, "mesh", accessor))
            return Containers::NullOpt;

        if(accessor->type != cgltf_type_scalar) {
            Error() << "Trade::CgltfImporter::mesh(): unexpected index type" << gltfTypeName(accessor->type);
            return Containers::NullOpt;
        }

        if(accessor->normalized) {
            Error() << "Trade::CgltfImporter::mesh(): index type can't be normalized";
            return Containers::NullOpt;
        }

        MeshIndexType type;
        if(accessor->component_type == cgltf_component_type_r_8u)
            type = MeshIndexType::UnsignedByte;
        else if(accessor->component_type == cgltf_component_type_r_16u)
            type = MeshIndexType::UnsignedShort;
        else if(accessor->component_type == cgltf_component_type_r_32u)
            type = MeshIndexType::UnsignedInt;
        else {
            Error{} << "Trade::CgltfImporter::mesh(): unexpected index component type" << gltfComponentTypeName(accessor->component_type);
            return Containers::NullOpt;
        }

        Containers::Optional<Containers::StridedArrayView2D<const char>> src = _d->accessorView(accessor, "mesh");
        if(!src)
            return Containers::NullOpt;

        if(!src->isContiguous()) {
            Error{} << "Trade::CgltfImporter::mesh(): index buffer view is not contiguous";
            return Containers::NullOpt;
        }

        Containers::ArrayView<const char> srcContiguous = src->asContiguous();
        indexData = Containers::Array<char>{srcContiguous.size()};
        Utility::copy(srcContiguous, indexData);
        indices = MeshIndexData{type, indexData};
    }

    /* If we have an index-less attribute-less mesh, glTF has no way to supply
       a vertex count, so return 0 */
    if(!indices.data().size() && !attributeData.size())
        return MeshData{meshPrimitive, 0};

    return MeshData{meshPrimitive,
        std::move(indexData), indices,
        std::move(vertexData), std::move(attributeData),
        vertexCount};
}

std::string CgltfImporter::doMeshAttributeName(UnsignedShort name) {
    return _d && name < _d->meshAttributeNames.size() ?
        _d->meshAttributeNames[name] : "";
}

MeshAttribute CgltfImporter::doMeshAttributeForName(const std::string& name) {
    return _d ? _d->meshAttributesForName[name] : MeshAttribute{};
}

UnsignedInt CgltfImporter::doMaterialCount() const {
    return _d->data->materials_count;
}

Int CgltfImporter::doMaterialForName(const std::string& name) {
    if(!_d->materialsForName) {
        _d->materialsForName.emplace();
        _d->materialsForName->reserve(_d->data->materials_count);
        for(std::size_t i = 0; i != _d->data->materials_count; ++i)
            _d->materialsForName->emplace(_d->decodeString(_d->data->materials[i].name), i);
    }

    const auto found = _d->materialsForName->find(name);
    return found == _d->materialsForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doMaterialName(const UnsignedInt id) {
    return _d->decodeString(_d->data->materials[id].name);
}

void CgltfImporter::Document::materialTexture(const cgltf_texture_view& texture, Containers::Array<MaterialAttributeData>& attributes, Containers::StringView attribute, Containers::StringView matrixAttribute, Containers::StringView coordinateAttribute) const {
    CORRADE_INTERNAL_ASSERT(texture.texture);

    UnsignedInt texCoord = texture.texcoord;

    /* Texture transform. Because texture coordinates were Y-flipped, we first
       unflip them back, apply the transform (which assumes origin at bottom
       left and Y down) and then flip the result again. Sanity of the following
       verified with https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/TextureTransformTest */
    if(texture.has_transform) {
        Matrix3 matrix;

        /* If material needs an Y-flip, the mesh doesn't have the texture
           coordinates flipped and thus we don't need to unflip them first */
        if(!textureCoordinateYFlipInMaterial)
            matrix = Matrix3::translation(Vector2::yAxis(1.0f))*
                     Matrix3::scaling(Vector2::yScale(-1.0f));

        /* The extension can override texture coordinate index (for example
           to have the unextended coordinates already transformed, and applying
           transformation to a different set) */
        if(texture.transform.has_texcoord)
            texCoord = texture.transform.texcoord;

        matrix = Matrix3::scaling(Vector2::from(texture.transform.scale))*matrix;

        /* Because we import images with Y flipped, counterclockwise rotation
           is now clockwise. This has to be done in addition to the Y
           flip/unflip. */
        matrix = Matrix3::rotation(-Rad(texture.transform.rotation))*matrix;

        matrix = Matrix3::translation(Vector2::from(texture.transform.offset))*matrix;

        matrix = Matrix3::translation(Vector2::yAxis(1.0f))*
                 Matrix3::scaling(Vector2::yScale(-1.0f))*matrix;

        arrayAppend(attributes, InPlaceInit, matrixAttribute, matrix);
    }

    /* In case the material had no texture transformation but still needs an
       Y-flip, put it there */
    if(!texture.has_transform && textureCoordinateYFlipInMaterial) {
        arrayAppend(attributes, InPlaceInit, matrixAttribute,
            Matrix3::translation(Vector2::yAxis(1.0f))*
            Matrix3::scaling(Vector2::yScale(-1.0f)));
    }

    /* Add texture coordinate set if non-zero. The KHR_texture_transform could
       be modifying it, so do that after */
    if(texCoord != 0)
        arrayAppend(attributes, InPlaceInit, coordinateAttribute, texCoord);

    /* In some cases (when dealing with packed textures), we're parsing &
       adding texture coordinates and matrix multiple times, but adding the
       packed texture ID just once. In other cases the attribute is invalid. */
    if(!attribute.isEmpty()) {
        const UnsignedInt textureId = texture.texture - data->textures;
        arrayAppend(attributes, InPlaceInit, attribute, textureId);
    }
}

Containers::Optional<MaterialData> CgltfImporter::doMaterial(const UnsignedInt id) {
    const cgltf_material& material = _d->data->materials[id];

    Containers::Array<UnsignedInt> layers;
    Containers::Array<MaterialAttributeData> attributes;
    MaterialTypes types;

    /* Alpha mode and mask, double sided */
    if(material.alpha_mode == cgltf_alpha_mode_blend)
        arrayAppend(attributes, InPlaceInit, MaterialAttribute::AlphaBlend, true);
    else if(material.alpha_mode == cgltf_alpha_mode_mask)
        arrayAppend(attributes, InPlaceInit, MaterialAttribute::AlphaMask, material.alpha_cutoff);
    else if(material.alpha_mode != cgltf_alpha_mode_opaque) {
        /* This should never be reached, cgltf treats invalid alpha modes as opaque */
        CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    if(material.double_sided)
        arrayAppend(attributes, InPlaceInit, MaterialAttribute::DoubleSided, true);

    /* Core metallic/roughness material */
    if(material.has_pbr_metallic_roughness) {
        types |= MaterialType::PbrMetallicRoughness;

        const Vector4 baseColorFactor = Vector4::from(material.pbr_metallic_roughness.base_color_factor);
        if(baseColorFactor != Vector4{1.0f})
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::BaseColor,
                Color4{baseColorFactor});
        if(material.pbr_metallic_roughness.metallic_factor != 1.0f)
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Metalness,
                material.pbr_metallic_roughness.metallic_factor);
        if(material.pbr_metallic_roughness.roughness_factor != 1.0f)
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Roughness,
                material.pbr_metallic_roughness.roughness_factor);

        if(material.pbr_metallic_roughness.base_color_texture.texture) {
            _d->materialTexture(
                material.pbr_metallic_roughness.base_color_texture,
                attributes,
                "BaseColorTexture"_s,
                "BaseColorTextureMatrix"_s,
                "BaseColorTextureCoordinates"_s);
        }

        if(material.pbr_metallic_roughness.metallic_roughness_texture.texture) {
            _d->materialTexture(
                material.pbr_metallic_roughness.metallic_roughness_texture,
                attributes,
                "NoneRoughnessMetallicTexture"_s,
                "MetalnessTextureMatrix"_s,
                "MetalnessTextureCoordinates"_s);

            /* Add the matrix/coordinates attributes also for the roughness
               texture, but skip adding the texture ID again */
            _d->materialTexture(
                material.pbr_metallic_roughness.metallic_roughness_texture,
                attributes,
                {},
                "RoughnessTextureMatrix"_s,
                "RoughnessTextureCoordinates"_s);
        }

        /** @todo Support for KHR_materials_specular? This adds an explicit
            F0 (texture) and a scalar factor (texture) for the entire specular
            reflection to a metallic/roughness material. */
    }

    /* Specular/glossiness material */
    if(material.has_pbr_specular_glossiness) {
        types |= MaterialType::PbrSpecularGlossiness;

        const Vector4 diffuseFactor = Vector4::from(material.pbr_specular_glossiness.diffuse_factor);
        if(diffuseFactor != Vector4{1.0f})
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::DiffuseColor,
                Color4{diffuseFactor});

        const Vector3 specularFactor = Vector3::from(material.pbr_specular_glossiness.specular_factor);
        if(specularFactor != Vector3{1.0f})
            arrayAppend(attributes, InPlaceInit,
                /* Specular is 3-component in glTF, alpha should be 0 to not
                   affect transparent materials */
                MaterialAttribute::SpecularColor,
                Color4{specularFactor, 0.0f});

        if(material.pbr_specular_glossiness.glossiness_factor != 1.0f)
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Glossiness,
                material.pbr_specular_glossiness.glossiness_factor);

        if(material.pbr_specular_glossiness.diffuse_texture.texture) {
            _d->materialTexture(
                material.pbr_specular_glossiness.diffuse_texture,
                attributes,
                "DiffuseTexture"_s,
                "DiffuseTextureMatrix"_s,
                "DiffuseTextureCoordinates"_s);
        }

        if(material.pbr_specular_glossiness.specular_glossiness_texture.texture) {
           _d->materialTexture(
                material.pbr_specular_glossiness.specular_glossiness_texture,
                attributes,
                "SpecularGlossinessTexture"_s,
                "SpecularTextureMatrix"_s,
                "SpecularTextureCoordinates"_s);

            /* Add the matrix/coordinates attributes also for the glossiness
               texture, but skip adding the texture ID again */
            _d->materialTexture(
                material.pbr_specular_glossiness.specular_glossiness_texture,
                attributes,
                {},
                "GlossinessTextureMatrix"_s,
                "GlossinessTextureCoordinates"_s);
        }
    }

    /* Unlit material -- reset all types and add just Flat */
    if(material.unlit)
        types = MaterialType::Flat;

    /* Normal texture */
    if(material.normal_texture.texture) {
        _d->materialTexture(
            material.normal_texture,
            attributes,
            "NormalTexture"_s,
            "NormalTextureMatrix"_s,
            "NormalTextureCoordinates"_s);

        if(material.normal_texture.scale != 1.0f)
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::NormalTextureScale,
                material.normal_texture.scale);
    }

    /* Occlusion texture */
    if(material.occlusion_texture.texture) {
        _d->materialTexture(
            material.occlusion_texture,
            attributes,
            "OcclusionTexture"_s,
            "OcclusionTextureMatrix"_s,
            "OcclusionTextureCoordinates"_s);

        /* cgltf exposes the strength multiplier as scale */
        if(material.occlusion_texture.scale != 1.0f)
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::OcclusionTextureStrength,
                material.occlusion_texture.scale);
    }

    /* Emissive factor & texture */
    const Vector3 emissiveFactor = Vector3::from(material.emissive_factor);
    if(emissiveFactor != Vector3{0.0f})
        arrayAppend(attributes, InPlaceInit,
            MaterialAttribute::EmissiveColor,
            Color3{emissiveFactor});
    if(material.emissive_texture.texture) {
        _d->materialTexture(
            material.emissive_texture,
            attributes,
            "EmissiveTexture"_s,
            "EmissiveTextureMatrix"_s,
            "EmissiveTextureCoordinates"_s);
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
            if(attribute.name() == "BaseColor")
                diffuseColor = attribute.value<Color4>();
            else if(attribute.name() == "BaseColorTexture")
                diffuseTexture = attribute.value<UnsignedInt>();
            else if(attribute.name() == "BaseColorTextureMatrix")
                diffuseTextureMatrix = attribute.value<Matrix3>();
            else if(attribute.name() == "BaseColorTextureCoordinates")
                diffuseTextureCoordinates = attribute.value<UnsignedInt>();
        }

        /* But if there already are those from the specular/glossiness
           material, don't add them again. Has to be done in a separate pass
           to avoid resetting too early. */
        for(const MaterialAttributeData& attribute: attributes) {
            if(attribute.name() == "DiffuseColor")
                diffuseColor = Containers::NullOpt;
            else if(attribute.name() == "DiffuseTexture")
                diffuseTexture = Containers::NullOpt;
            else if(attribute.name() == "DiffuseTextureMatrix")
                diffuseTextureMatrix = Containers::NullOpt;
            else if(attribute.name() == "DiffuseTextureCoordinates")
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

    /* Clear coat layer -- needs to be after all base material attributes */
    if(material.has_clearcoat) {
        types |= MaterialType::PbrClearCoat;

        /* Add a new layer -- this works both if layers are empty and if
           there's something already */
        arrayAppend(layers, UnsignedInt(attributes.size()));
        arrayAppend(attributes, InPlaceInit, MaterialLayer::ClearCoat);

        arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::LayerFactor,
                material.clearcoat.clearcoat_factor);

        if(material.clearcoat.clearcoat_texture.texture) {
            _d->materialTexture(
                material.clearcoat.clearcoat_texture,
                attributes,
                "LayerFactorTexture"_s,
                "LayerFactorTextureMatrix"_s,
                "LayerFactorTextureCoordinates"_s);
        }

        arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::Roughness,
                material.clearcoat.clearcoat_roughness_factor);

        if(material.clearcoat.clearcoat_roughness_texture.texture) {
            _d->materialTexture(
                material.clearcoat.clearcoat_roughness_texture,
                attributes,
                "RoughnessTexture"_s,
                "RoughnessTextureMatrix"_s,
                "RoughnessTextureCoordinates"_s);

            /* The extension description doesn't mention it, but the schema
               says the clearcoat roughness is actually in the G channel:
               https://github.com/KhronosGroup/glTF/blob/dc5519b9ce9834f07c30ec4c957234a0cd6280a2/extensions/2.0/Khronos/KHR_materials_clearcoat/schema/glTF.KHR_materials_clearcoat.schema.json#L32 */
            arrayAppend(attributes, InPlaceInit,
                MaterialAttribute::RoughnessTextureSwizzle,
                MaterialTextureSwizzle::G);
        }

        if(material.clearcoat.clearcoat_normal_texture.texture) {
            _d->materialTexture(
                material.clearcoat.clearcoat_normal_texture,
                attributes,
                "NormalTexture"_s,
                "NormalTextureMatrix"_s,
                "NormalTextureCoordinates"_s);

            if(material.clearcoat.clearcoat_normal_texture.scale != 1.0f)
                arrayAppend(attributes, InPlaceInit,
                    MaterialAttribute::NormalTextureScale,
                    material.clearcoat.clearcoat_normal_texture.scale);
        }
    }

    /* If there's any layer, add the final attribute count */
    arrayAppend(layers, UnsignedInt(attributes.size()));

    /* Can't use growable deleters in a plugin, convert back to the default
       deleter */
    arrayShrink(layers);
    arrayShrink(attributes, DefaultInit);
    return MaterialData{types, std::move(attributes), std::move(layers)};
}

UnsignedInt CgltfImporter::doTextureCount() const {
    return _d->data->textures_count;
}

Int CgltfImporter::doTextureForName(const std::string& name) {
    if(!_d->texturesForName) {
        _d->texturesForName.emplace();
        _d->texturesForName->reserve(_d->data->textures_count);
        for(std::size_t i = 0; i != _d->data->textures_count; ++i)
            _d->texturesForName->emplace(_d->decodeString(_d->data->textures[i].name), i);
    }

    const auto found = _d->texturesForName->find(name);
    return found == _d->texturesForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doTextureName(const UnsignedInt id) {
    return _d->decodeString(_d->data->textures[id].name);
}

Containers::Optional<TextureData> CgltfImporter::doTexture(const UnsignedInt id) {
    const cgltf_texture& tex = _d->data->textures[id];

    UnsignedInt imageId = ~0u;

    /* Various extensions, they override the standard image */
    if(tex.has_basisu && tex.basisu_image) {
        /* KHR_texture_basisu. Allows the usage of mimeType image/ktx2 but only
           explicitly talks about KTX2 with Basis compression. We don't care
           since we delegate to AnyImageImporter and let it figure out the file
           type based on magic. Note: The core glTF spec only allows image/jpeg
           and image/png but we don't check that either. */
        imageId = tex.basisu_image - _d->data->images;
    } else {
        constexpr Containers::StringView extensions[]{
            /* GOOGLE_texture_basis is not a registered extension but can be found
               in some of the early Basis Universal examples. Basis files don't
               have a registered mimetype either, but as explained above we don't
               care about mimetype at all. */
            "GOOGLE_texture_basis"_s,
            "MSFT_texture_dds"_s
            /** @todo EXT_texture_webp once a plugin provides WebpImporter */
        };
        /* Use the first supported extension, assuming that extension order
           indicates a preference */
        /** @todo Figure out a better priority
            - extensionsRequired?
            - image importers available via manager()->aliasList()?
            - are there even files out there with more than one extension? */
        for(std::size_t i = 0; i != tex.extensions_count && imageId == ~0u; ++i) {
            for(const auto& ext: extensions) {
                if(tex.extensions[i].name == ext) {
                    const Containers::StringView json = tex.extensions[i].data;
                    const auto tokens = parseJson(tex.extensions[i].data);
                    /* This is checked by cgltf */
                    CORRADE_INTERNAL_ASSERT(!tokens.empty() && tokens[0].type == JSMN_OBJECT);
                    /* @todo This assumes there is only one key in the object.
                       There could be unrelated things in there like "note". */
                    if(tokens.size() == 3 && tokens[1].type == JSMN_STRING &&
                       tokenString(json, tokens[1]) == "source" && tokens[2].type == JSMN_PRIMITIVE)
                    {
                        const Int source = cgltf_json_to_int(&tokens[2], reinterpret_cast<const uint8_t*>(json.data()));
                        if(source < 0 || UnsignedInt(source) >= _d->data->images_count) {
                            Error{} << "Trade::CgltfImporter::texture():" << ext << "image" << source << "out of bounds for" << _d->data->images_count << "images";
                            return Containers::NullOpt;
                        }
                        imageId = source;
                    }
                    break;
                }
            }
        }
    }

    if(imageId == ~0u) {
        /* If not overwritten by an extension, use the standard 'source'
           attribute. It's not mandatory, so this can still fail. */
        if(tex.image)
            imageId = tex.image - _d->data->images;
        else {
            Error{} << "Trade::CgltfImporter::texture(): no image source found";
            return Containers::NullOpt;
        }
    }

    CORRADE_INTERNAL_ASSERT(imageId < _d->data->images_count);

    /* Sampler */
    if(!tex.sampler) {
        /* The specification instructs to use "auto sampling", i.e. it is left
           to the implementor to decide on the default values... */
        return TextureData{TextureType::Texture2D, SamplerFilter::Linear, SamplerFilter::Linear,
            SamplerMipmap::Linear, {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat}, imageId};
    }

    /* GL filter enums */
    enum GltfTextureFilter: cgltf_int {
        Nearest = 9728,
        Linear = 9729,
        NearestMipmapNearest = 9984,
        LinearMipmapNearest = 9985,
        NearestMipmapLinear = 9986,
        LinearMipmapLinear = 9987
    };

    SamplerFilter minFilter;
    SamplerMipmap mipmap;
    switch(tex.sampler->min_filter) {
        case GltfTextureFilter::Nearest:
            minFilter = SamplerFilter::Nearest;
            mipmap = SamplerMipmap::Base;
            break;
        case GltfTextureFilter::Linear:
            minFilter = SamplerFilter::Linear;
            mipmap = SamplerMipmap::Base;
            break;
        case GltfTextureFilter::NearestMipmapNearest:
            minFilter = SamplerFilter::Nearest;
            mipmap = SamplerMipmap::Nearest;
            break;
        case GltfTextureFilter::NearestMipmapLinear:
            minFilter = SamplerFilter::Nearest;
            mipmap = SamplerMipmap::Linear;
            break;
        case GltfTextureFilter::LinearMipmapNearest:
            minFilter = SamplerFilter::Linear;
            mipmap = SamplerMipmap::Nearest;
            break;
        case GltfTextureFilter::LinearMipmapLinear:
        /* glTF 2.0 spec does not define a default value for 'minFilter' and
           'magFilter'. In this case cgltf sets it to 0. */
        case 0:
            minFilter = SamplerFilter::Linear;
            mipmap = SamplerMipmap::Linear;
            break;
        default:
            Error{} << "Trade::CgltfImporter::texture(): invalid minFilter" << tex.sampler->min_filter;
            return Containers::NullOpt;
    }

    SamplerFilter magFilter;
    switch(tex.sampler->mag_filter) {
        case GltfTextureFilter::Nearest:
            magFilter = SamplerFilter::Nearest;
            break;
        case GltfTextureFilter::Linear:
        /* glTF 2.0 spec does not define a default value for 'minFilter' and
           'magFilter'. In this case cgltf sets it to 0. */
        case 0:
            magFilter = SamplerFilter::Linear;
            break;
        default:
            Error{} << "Trade::CgltfImporter::texture(): invalid magFilter" << tex.sampler->mag_filter;
            return Containers::NullOpt;
    }

    /* GL wrap enums */
    enum GltfTextureWrap: cgltf_int {
        Repeat = 10497,
        ClampToEdge = 33071,
        MirroredRepeat = 33648
    };

    Math::Vector3<SamplerWrapping> wrapping;
    wrapping.z() = SamplerWrapping::Repeat;
    for(auto&& wrap: std::initializer_list<Containers::Pair<Int, Int>>{
        {tex.sampler->wrap_s, 0}, {tex.sampler->wrap_t, 1}})
    {
        switch(wrap.first()) {
            case GltfTextureWrap::Repeat:
                wrapping[wrap.second()] = SamplerWrapping::Repeat;
                break;
            case GltfTextureWrap::ClampToEdge:
                wrapping[wrap.second()] = SamplerWrapping::ClampToEdge;
                break;
            case GltfTextureWrap::MirroredRepeat:
                wrapping[wrap.second()] = SamplerWrapping::MirroredRepeat;
                break;
            default:
                Error{} << "Trade::CgltfImporter::texture(): invalid wrap mode" << wrap.first();
                return Containers::NullOpt;
        }
    }

    /* glTF supports only 2D textures */
    return TextureData{TextureType::Texture2D, minFilter, magFilter,
        mipmap, wrapping, imageId};
}

UnsignedInt CgltfImporter::doImage2DCount() const {
    return _d->data->images_count;
}

Int CgltfImporter::doImage2DForName(const std::string& name) {
    if(!_d->imagesForName) {
        _d->imagesForName.emplace();
        _d->imagesForName->reserve(_d->data->images_count);
        for(std::size_t i = 0; i != _d->data->images_count; ++i)
            _d->imagesForName->emplace(_d->decodeString(_d->data->images[i].name), i);
    }

    const auto found = _d->imagesForName->find(name);
    return found == _d->imagesForName->end() ? -1 : found->second;
}

std::string CgltfImporter::doImage2DName(const UnsignedInt id) {
    return _d->decodeString(_d->data->images[id].name);
}

AbstractImporter* CgltfImporter::setupOrReuseImporterForImage(const UnsignedInt id, const char* const function) {
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

    const cgltf_image& image = _d->data->images[id];

    /* Load embedded image. Can either be a buffer view or a base64 payload.
       Buffers are kept in memory until the importer closes but decoded base64
       data is freed after opening the image. */
    if(!image.uri || isDataUri(image.uri)) {
        Containers::Array<char> imageData;
        Containers::ArrayView<const char> imageView;

        if(image.uri) {
            const auto view = _d->loadUri(image.uri, imageData, function);
            if(!view)
                return nullptr;
            imageView = *view;
        } else {
            if(!image.buffer_view) {
                Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): image has neither a URI nor a buffer view";
                return nullptr;
            }

            const cgltf_buffer* buffer = image.buffer_view->buffer;
            const UnsignedInt bufferId = buffer - _d->data->buffers;
            if(!_d->loadBuffer(bufferId, function))
                return nullptr;
            imageView = Containers::arrayView(static_cast<const char*>(buffer->data) + image.buffer_view->offset, image.buffer_view->size);
        }

        if(!importer.openData(imageView))
            return nullptr;
        return &_d->imageImporter.emplace(std::move(importer));
    }

    /* Load external image */
    if(!_d->filePath && !fileCallback()) {
        Error{} << "Trade::CgltfImporter::" << Debug::nospace << function << Debug::nospace << "(): external images can be imported only when opening files from the filesystem or if a file callback is present";
        return nullptr;
    }

    if(!importer.openFile(Utility::Directory::join(_d->filePath ? *_d->filePath : "", decodeUri(_d->decodeString(image.uri)))))
        return nullptr;
    return &_d->imageImporter.emplace(std::move(importer));
}

UnsignedInt CgltfImporter::doImage2DLevelCount(const UnsignedInt id) {
    CORRADE_ASSERT(manager(), "Trade::CgltfImporter::image2DLevelCount(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    AbstractImporter* importer = setupOrReuseImporterForImage(id, "image2DLevelCount");
    /* image2DLevelCount() isn't supposed to fail (image2D() is, instead), so
       report 1 on failure and expect image2D() to fail later */
    if(!importer) return 1;

    return importer->image2DLevelCount(0);
}

Containers::Optional<ImageData2D> CgltfImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    CORRADE_ASSERT(manager(), "Trade::CgltfImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to load images", {});

    AbstractImporter* importer = setupOrReuseImporterForImage(id, "image2D");
    if(!importer) return Containers::NullOpt;

    Containers::Optional<ImageData2D> imageData = importer->image2D(0, level);
    if(!imageData) return Containers::NullOpt;
    return ImageData2D{std::move(*imageData)};
}

}}

CORRADE_PLUGIN_REGISTER(CgltfImporter, Magnum::Trade::CgltfImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.4")
