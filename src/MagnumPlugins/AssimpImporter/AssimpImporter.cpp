/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017, 2020, 2021, 2022 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2018 Konstantinos Chatzilygeroudis <costashatz@gmail.com>
    Copyright © 2019, 2020 Max Schwarz <max.schwarz@ais.uni-bonn.de>
    Copyright © 2021, 2022 Pablo Escobar <mail@rvrs.in>

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

#include "AssimpImporter.h"

#include <cctype>
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayTuple.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/BigEnumSet.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringStlHash.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Macros.h> /* _CORRADE_HELPER_DEFER */
#include <Corrade/Utility/Path.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/PixelStorage.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/ArrayAllocator.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/SkinData.h>
#include <Magnum/Trade/TextureData.h>
#include <MagnumPlugins/AnyImageImporter/AnyImageImporter.h>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Logger.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#include "configureInternal.h"

namespace Corrade { namespace Containers { namespace Implementation {

template<> struct StringConverter<aiString> {
    static String from(const aiString& other) {
        return String{other.C_Str(), other.length};
    }
};

template<> struct StringViewConverter<const char, aiString> {
    static StringView from(const aiString& other) {
        return StringView{other.C_Str(), other.length, Containers::StringViewFlag::NullTerminated};
    }
};

}}}

namespace Magnum { namespace Math { namespace Implementation {

template<> struct VectorConverter<2, Float, aiVector2D> {
    static Vector<2, Float> from(const aiVector2D& other) {
        return {other.x, other.y};
    }
};

template<> struct VectorConverter<3, Float, aiColor3D> {
    static Vector<3, Float> from(const aiColor3D& other) {
        return {other.r, other.g, other.b};
    }
};

template<> struct VectorConverter<4, Float, aiColor4D> {
    static Vector<4, Float> from(const aiColor4D& other) {
        return {other.r, other.g, other.b, other.a};
    }
};

}}}

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

struct AssimpImporter::File {
    Containers::Optional<Containers::String> filePath;
    UnsignedInt assimpVersion = 0;
    bool importerIsGltf = false;
    const aiScene* scene = nullptr;
    /* Index -> pointer, pointer -> index conversion for nodes as they're
       represented in a tree. Needed by objectCount() (which is const) so can't
       be delayed to scenne() parsing. */
    Containers::Array<const aiNode*> nodes;
    std::unordered_map<const aiNode*, UnsignedInt> nodeIndices;
    /* (materialPointer, propertyIndexInsideMaterial, imageIndex) tuple,
       imageIndex points to the (deduplicated) images array */
    Containers::Array<Containers::Triple<const aiMaterial*, UnsignedInt, UnsignedInt>> textures;
    /* (materialPointer, propertyIndexInsideMaterial) tuple defining the first
       (unique) location of an image */
    Containers::Array<Containers::Pair<const aiMaterial*, UnsignedInt>> images;

    std::unordered_map<Containers::String, UnsignedInt> materialIndicesForName;
    std::unordered_map<const aiMaterial*, UnsignedInt> textureIndices;

    /* We can use StringView for the keys because they point to aiString mName
       in the scene, which stay valid for as long as a file is opened */
    Containers::Optional<std::unordered_map<Containers::StringView, Int>>
        animationsForName,
        camerasForName,
        lightsForName,
        meshesForName,
        skinsForName;

    Containers::Array<std::size_t> meshesWithBones;
    /* For each mesh: the index of its skin (before any merging), or -1 */
    Containers::Array<Int> meshSkins;
    bool mergeSkins = false;
    /* For each mesh, map from mesh-relative bones to merged bone list */
    Containers::Array<Containers::Array<UnsignedInt>> boneMap;
    Containers::Array<const aiBone*> mergedBones;

    UnsignedInt imageImporterId = ~UnsignedInt{};
    Containers::Optional<AnyImageImporter> imageImporter;

    Matrix4 rootTransformation;
};

namespace {

#ifdef MAGNUM_BUILD_DEPRECATED
/* Joint ids and weights are the only custom attributes in this importer */
constexpr MeshAttribute JointsAttribute = meshAttributeCustom(0);
constexpr MeshAttribute WeightsAttribute = meshAttributeCustom(1);
#endif

void fillDefaultConfiguration(Utility::ConfigurationGroup& conf) {
    /** @todo horrible workaround, fix this properly */

    conf.setValue("forceWhiteAmbientToBlack", true);

    conf.setValue("optimizeQuaternionShortestPath", true);
    conf.setValue("normalizeQuaternions", true);
    conf.setValue("mergeAnimationClips", false);
    conf.setValue("removeDummyAnimationTracks", true);
    conf.setValue("maxJointWeights", 4);
    conf.setValue("mergeSkins", false);

    conf.setValue("ImportColladaIgnoreUpDirection", false);
    conf.setValue("ignoreUnrecognizedMaterialData", false);
    conf.setValue("forceRawMaterialData", false);

    Utility::ConfigurationGroup& postprocess = *conf.addGroup("postprocess");
    postprocess.setValue("JoinIdenticalVertices", true);
    postprocess.setValue("Triangulate", true);
    postprocess.setValue("SortByPType", true);
}

Containers::Pointer<Assimp::Importer> createImporter(Utility::ConfigurationGroup& conf) {
    Containers::Pointer<Assimp::Importer> importer{InPlaceInit};

    /* Without this setting, Assimp adds bogus skeleton visualization meshes
       to files that don't have any meshes. It claims to only do this when
       there is animation data, but at least for Collada it always does it. */
    importer->SetPropertyBool(AI_CONFIG_IMPORT_NO_SKELETON_MESHES, true);

    importer->SetPropertyBool(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION,
        conf.value<bool>("ImportColladaIgnoreUpDirection"));
    importer->SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS,
        conf.value<int>("maxJointWeights"));

    return importer;
}

std::size_t countNodeDescendants(const aiNode* node) {
    std::size_t count = node->mNumChildren;
    for(aiNode* child: Containers::arrayView(node->mChildren, node->mNumChildren)) {
        count += countNodeDescendants(child);
    }
    return count;
}

bool isDummyBone(const aiBone* bone) {
    /* Dummy bone weights inserted by Assimp:
       https://github.com/assimp/assimp/blob/1d33131e902ff3f6b571ee3964c666698a99eb0f/code/AssetLib/glTF2/glTF2Importer.cpp#L1099 */
    return bone->mNumWeights == 1 &&
        bone->mWeights[0].mVertexId == 0 &&
        Math::equal(bone->mWeights[0].mWeight, 0.0f);
}

}

AssimpImporter::AssimpImporter() {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

AssimpImporter::AssimpImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter(manager) {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

AssimpImporter::AssimpImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter(manager, plugin) {}

AssimpImporter::~AssimpImporter() {
    /* Because we are dealing with a crappy singleton here, we need to make
       sure to clean up everything that might have been set earlier */
    /** @todo wait how does this work with multiple simultaenous instances?! */
    if(flags() & ImporterFlag::Verbose) Assimp::DefaultLogger::kill();
}

ImporterFeatures AssimpImporter::doFeatures() const { return ImporterFeature::OpenData|ImporterFeature::OpenState|ImporterFeature::FileCallback; }

bool AssimpImporter::doIsOpened() const { return _f && _f->scene; }

namespace {

struct IoStream: Assimp::IOStream {
    explicit IoStream(std::string filename, Containers::ArrayView<const char> data): filename{std::move(filename)}, _data{data}, _pos{} {}

    /* mimics fread() / fseek() */
    std::size_t Read(void* buffer, std::size_t size, std::size_t count) override {
        /* For some zero-sized files, Assimp passes zero size. Ensure we don't
           crash on a division-by-zero. */
        if(!size) return 0;

        const Containers::ArrayView<const char> slice = _data.exceptPrefix(_pos);
        const std::size_t maxCount = Math::min(slice.size()/size, count);
        std::memcpy(buffer, slice.begin(), size*maxCount);
        _pos += size*maxCount;
        return maxCount;
    }
    aiReturn Seek(std::size_t offset, aiOrigin origin) override {
        if(origin == aiOrigin_SET && offset < _data.size())
            _pos = offset;
        else if(origin == aiOrigin_CUR && _pos + offset < _data.size())
            _pos += offset;
        else if(origin == aiOrigin_END && _data.size() + std::ptrdiff_t(offset) < _data.size())
            _pos = _data.size() + std::ptrdiff_t(offset);
        else return aiReturn_FAILURE;
        return aiReturn_SUCCESS;
    }
    std::size_t Tell() const override { return _pos; }
    std::size_t FileSize() const override { return _data.size(); }

    /* We are just a reader */
    /* LCOV_EXCL_START */
    std::size_t Write(const void*, std::size_t, std::size_t) override {
        CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    }
    void Flush() override {
        CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    }
    /* LCOV_EXCL_STOP */

    std::string filename; /* needed for closing properly on the user side */
    private:
        Containers::ArrayView<const char> _data;
        std::size_t _pos;
};

struct IoSystem: Assimp::IOSystem {
    explicit IoSystem(Containers::Optional<Containers::ArrayView<const char>>(*callback)(const std::string&, InputFileCallbackPolicy, void*), void* userData): _callback{callback}, _userData{userData} {}

    /* What else? I can't know. */
    bool Exists(const char*) const override { return true; }

    /* The file callback on user side has to deal with this */
    char getOsSeparator() const override { return '/'; }

    Assimp::IOStream* Open(const char* file, const char* mode) override {
        CORRADE_INTERNAL_ASSERT(mode == "rb"_s);
        #ifdef CORRADE_NO_ASSERT
        static_cast<void>(mode);
        #endif
        const Containers::Optional<Containers::ArrayView<const char>> data = _callback(file, InputFileCallbackPolicy::LoadTemporary, _userData);
        if(!data) return {};
        return new IoStream{file, *data};
    }

    void Close(Assimp::IOStream* file) override {
        _callback(static_cast<IoStream*>(file)->filename, InputFileCallbackPolicy::Close, _userData);
        delete file;
    }

    Containers::Optional<Containers::ArrayView<const char>>(*_callback)(const std::string&, InputFileCallbackPolicy, void*);
    void* _userData;
};

}

void AssimpImporter::doSetFlags(const ImporterFlags flags) {
    struct DebugStream: Assimp::LogStream {
        void write(const char* message) override {
            Debug{Debug::Flag::NoNewlineAtTheEnd} << "Trade::AssimpImporter:" << message;
        }
    };

    /* I'm extremely unsure about leaks, memory ownership, or whether this
       really restores things back to the default. Ugh, what's the obsession
       with extremely complex loggers everywhere? If a thing works, you don't
       need gigabytes of logs vomitted from every function calls. */
    if(flags & ImporterFlag::Verbose) {
        Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
        Assimp::DefaultLogger::get()->attachStream(new DebugStream,
            Assimp::Logger::Info|Assimp::Logger::Err|Assimp::Logger::Warn|Assimp::Logger::Debugging);
    } else Assimp::DefaultLogger::kill();
}

void AssimpImporter::doSetFileCallback(Containers::Optional<Containers::ArrayView<const char>>(*callback)(const std::string&, InputFileCallbackPolicy, void*), void* userData) {
    if(!_importer) _importer = createImporter(configuration());

    if(callback) {
        _importer->SetIOHandler(_ourFileCallback = new IoSystem{callback, userData});

    /* Passing nullptr to Assimp will deliberately leak the previous IOSystem
       instance (whereas passing a non-null instance will delete the previous
       one). That means we need to keep track of our file callbacks and delete
       them manually. */
    } else if(_importer->GetIOHandler() == _ourFileCallback) {
        delete _ourFileCallback;
        _importer->SetIOHandler(nullptr);
        _ourFileCallback = nullptr;
    }
}

namespace {

UnsignedInt flagsFromConfiguration(Utility::ConfigurationGroup& conf) {
    UnsignedInt flags = 0;
    if(conf.value<UnsignedInt>("maxJointWeights"))
        flags |= aiProcess_LimitBoneWeights;

    const Utility::ConfigurationGroup& postprocess = *conf.group("postprocess");
    #define _c(val) if(postprocess.value<bool>(#val)) flags |= aiProcess_ ## val;
    _c(CalcTangentSpace)
    /* Without aiProcess_JoinIdenticalVertices all meshes are deindexed (wtf?) */
    _c(JoinIdenticalVertices) /* enabled by default */
    _c(Triangulate) /* enabled by default */
    _c(GenNormals)
    _c(GenSmoothNormals)
    _c(SplitLargeMeshes)
    _c(PreTransformVertices)
    _c(ValidateDataStructure)
    _c(ImproveCacheLocality)
    _c(RemoveRedundantMaterials)
    _c(FixInfacingNormals)
    _c(SortByPType) /* enabled by default */
    _c(FindDegenerates)
    _c(FindInvalidData)
    _c(GenUVCoords)
    _c(TransformUVCoords)
    _c(FindInstances)
    _c(OptimizeMeshes)
    _c(OptimizeGraph)
    _c(FlipUVs)
    _c(FlipWindingOrder)
    _c(SplitByBoneCount)
    _c(Debone)
    #undef _c
    return flags;
}

/* Assimp doesn't implement any getters directly on a material property (only a
   lookup via key on aiMaterial), so here's a copy of aiGetMaterialString()
   internals: https://github.com/assimp/assimp/blob/e845988c22d449b3fe45c1e96d51ae2fa6b59979/code/Material/MaterialSystem.cpp#L299-L306 */
Containers::StringView materialPropertyString(const aiMaterialProperty& property) {
    CORRADE_INTERNAL_ASSERT(property.mType == aiPTI_String);
    /* The string is stored with 32-bit length prefix followed by a
       null-terminated data, and according to asserts in aiGetMaterialString()
       the total length should correspond with mDataLength, so just assert
       that here and use mDataLength instead as that doesn't need any ugly
       casts */
    CORRADE_INTERNAL_ASSERT(*reinterpret_cast<UnsignedInt*>(property.mData) + 1 + 4 == property.mDataLength);
    return {property.mData + 4, property.mDataLength - 4 - 1, Containers::StringViewFlag::NullTerminated};
}

}

void AssimpImporter::doOpenData(Containers::Array<char>&& data, DataFlags) {
    /* If we already have the file, we got delegated from doOpenFile() or
       doOpenState(). If we got called from doOpenState(), we don't even have
       the _importer. No need to create it. */
    if(!_f) {
        if(!_importer) _importer = createImporter(configuration());

        _f.reset(new File);
        /* File callbacks are set up in doSetFileCallbacks() */
        if(!(_f->scene = _importer->ReadFileFromMemory(data.data(), data.size(), flagsFromConfiguration(configuration())))) {
            Error{} << "Trade::AssimpImporter::openData(): loading failed:" << _importer->GetErrorString();
            return;
        }
    }

    CORRADE_INTERNAL_ASSERT(_f->scene);

    _f->assimpVersion = aiGetVersionMajor()*100 + aiGetVersionMinor()*10
        #if ASSIMP_HAS_VERSION_PATCH
        + aiGetVersionPatch()
        #endif
        ;

    /* Assimp 5.0.0 reports itself as 4.1.0 */
    #if ASSIMP_IS_VERSION_5_OR_GREATER
    _f->assimpVersion =  Math::max(_f->assimpVersion, 500u);
    #endif

    /* Get name of importer. Useful for workarounds based on importer/file
       type. If the _importer isn't populated, we got called from doOpenState()
       and we can't really do much here. */
    if(_importer) {
        const int importerIndex = _importer->GetPropertyInteger("importerIndex", -1);
        if(importerIndex != -1) {
            const aiImporterDesc* info = _importer->GetImporterInfo(importerIndex);
            if(info) {
                _f->importerIsGltf = info->mName == "glTF2 Importer"_s;
            }
        }
    }

    /* Fill hashmaps for index lookup for materials & textures */
    std::size_t numTextures = 0;
    for(aiMaterial* mat: Containers::arrayView(_f->scene->mMaterials, _f->scene->mNumMaterials))
        for(aiMaterialProperty* property: Containers::arrayView(mat->mProperties, mat->mNumProperties))
            if(Containers::StringView{property->mKey} == _AI_MATKEY_TEXTURE_BASE)
                ++numTextures;
    arrayReserve(_f->textures, numTextures);
    arrayReserve(_f->images, numTextures);
    _f->textureIndices.reserve(numTextures);
    _f->materialIndicesForName.reserve(_f->scene->mNumMaterials);

    aiString matName;
    UnsignedInt textureIndex = 0;
    std::unordered_map<Containers::StringView, UnsignedInt> uniqueImages;
    uniqueImages.reserve(numTextures);
    for(UnsignedInt i = 0; i < _f->scene->mNumMaterials; ++i) {
        const aiMaterial* mat = _f->scene->mMaterials[i];

        if(mat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
            /* Avoid a copy if we can. Can't insert with the non-owned String
               key because that'd be a dangling view. */
            auto found = _f->materialIndicesForName.find(Containers::String::nullTerminatedView(matName));
            if(found == _f->materialIndicesForName.end())
                _f->materialIndicesForName[matName] = i;
            else
                found->second = i;
        }

        /* Store first possible texture index for this material, next textures
           use successive indices. */
        _f->textureIndices[mat] = textureIndex;
        for(UnsignedInt j = 0; j != mat->mNumProperties; ++j) {
            /* We're only interested in AI_MATKEY_TEXTURE_* properties */
            const aiMaterialProperty& property = *mat->mProperties[j];
            if(Containers::StringView{property.mKey} != _AI_MATKEY_TEXTURE_BASE) continue;

            /* For images ensure we have an unique path so each file isn't
               imported more than once. Each image then points to j-th property
               of the material, which is then used to retrieve its path again. */
            const Containers::StringView texturePath = materialPropertyString(property);
            auto uniqueImage = uniqueImages.emplace(texturePath, _f->images.size());
            if(uniqueImage.second) arrayAppend(_f->images, InPlaceInit, mat, j);

            /* Each texture points to j-th property of the material, which is
               then used to retrieve related info, plus an index into the
               unique images array */
            arrayAppend(_f->textures, InPlaceInit, mat, j, uniqueImage.first->second);
            ++textureIndex;
        }
    }

    /* For some formats (such as COLLADA) Assimp fails to open the scene if
       there are no nodes, so there this is always non-null. For other formats
       (such as glTF) Assimp happily provides a null root node, even though
       that's not the documented behavior. */
    aiNode* const root = _f->scene->mRootNode;
    if(root) {
        /* If there is more than just a root node, extract children of the root
           node, as we treat the root node as the scene here. In some cases
           (for example for a COLLADA file with Z_UP defined) the root node can
           contain a transformation, save it. This root transformation is then
           applied to all direct children of mRootNode inside doObject3D().
           Apart from that it shouldn't contain anything else. */
        if(root->mNumChildren) {
            /* In case of openState() (which is when _importer isn't populated)
               it might happen that AI_CONFIG_IMPORT_NO_SKELETON_MESHES was not
               enabled, in which case, and at least for COLLADA, if the file
               has no meshes, it adds some bogus one, thinking it's a
               skeleton-only file and trying to be helpful. Ugh.
                https://github.com/assimp/assimp/blob/92078bc47c462d5b643aab3742a8864802263700/code/ColladaLoader.cpp#L225
               Don't die on the assert in this case, but otherwise check that
               we didn't miss anything in the root node. */
            CORRADE_INTERNAL_ASSERT(!_importer || !root->mNumMeshes);

            const std::size_t numDescendants = countNodeDescendants(root);
            arrayReserve(_f->nodes, numDescendants);
            arrayAppend(_f->nodes, Containers::arrayView(root->mChildren, root->mNumChildren));
            _f->nodeIndices.reserve(numDescendants);
            _f->rootTransformation = Matrix4::from(reinterpret_cast<const float*>(&root->mTransformation)).transposed();

        /* In various cases (PreTransformVertices enabled when the file has a
           mesh, COLLADA files with Z-up patching not set, STL files...)
           there's just one root node, and the DART integration depends on
           that. Import it as a single node. In this case applying the root
           transformation is not desired, so set it to identity. This branch is
           explicitly verified in the sceneCollapsedNode() test case. */
        } else {
            arrayAppend(_f->nodes, root);
            _f->rootTransformation = Matrix4{};
        }

        /* Unpack the node tree into a linear array we can index into. Insert
           into a vector may invalidate iterators, so we use indices here. */
        for(std::size_t i = 0; i < _f->nodes.size(); ++i) {
            const aiNode* node = _f->nodes[i];
            _f->nodeIndices[node] = UnsignedInt(i);
            arrayAppend(_f->nodes, Containers::arrayView(node->mChildren, node->mNumChildren));
        }
    }

    #if ASSIMP_HAS_BROKEN_GLTF_SPLINES
    /* Before https://github.com/assimp/assimp/commit/e3083c21f0a7beae6c37a2265b7919a02cbf83c4
       Assimp incorrectly read spline tangents as values in glTF animation
       tracks. Quick and dirty check to see if we're dealing with a possibly
       affected file and Assimp version. This might produce false-positives on
       files without spline-interpolated animations, but for doOpenState and
       doOpenFile we have no access to the file content to check if the file
       contains "CUBICSPLINE". */
    if(_f->scene->HasAnimations() && _f->importerIsGltf) {
        Warning{} << "Trade::AssimpImporter::openData(): spline-interpolated animations imported "
            "from this file are most likely broken using this version of Assimp. Consult the "
            "importer documentation for more information.";
    }
    #endif

    /* Find meshes with bone data, those are our skins */
    _f->meshSkins = Containers::Array<Int>{NoInit, _f->scene->mNumMeshes};
    arrayReserve(_f->meshesWithBones, _f->scene->mNumMeshes);
    for(std::size_t i = 0; i != _f->scene->mNumMeshes; ++i) {
        Int skin = -1;
        if(_f->scene->mMeshes[i]->HasBones()) {
            skin = _f->meshesWithBones.size();
            arrayAppend(_f->meshesWithBones, i);
        }
        _f->meshSkins[i] = skin;
    }

    /* Can't be changed per skin-import because it affects joint id vertex
       attributes during doMesh */
    _f->mergeSkins = configuration().value<bool>("mergeSkins");

    /* De-duplicate bones across all skinned meshes */
    if(_f->mergeSkins) {
        std::size_t numBones = 0;
        for(UnsignedInt mesh: _f->meshesWithBones)
            numBones += _f->scene->mMeshes[mesh]->mNumBones;
        arrayReserve(_f->mergedBones, numBones);

        _f->boneMap = Containers::Array<Containers::Array<UnsignedInt>>{ValueInit, _f->meshesWithBones.size()};
        for(std::size_t s = 0; s < _f->meshesWithBones.size(); ++s) {
            const UnsignedInt id = _f->meshesWithBones[s];
            const aiMesh* mesh = _f->scene->mMeshes[id];
            auto& map = _f->boneMap[s];
            arrayReserve(map, mesh->mNumBones);
            for(const aiBone* bone: Containers::arrayView(mesh->mBones, mesh->mNumBones)) {
                Int index = -1;
                for(std::size_t i = 0; i != _f->mergedBones.size(); ++i) {
                    const aiBone* other = _f->mergedBones[i];
                    if(bone->mName == other->mName &&
                        bone->mOffsetMatrix.Equal(other->mOffsetMatrix))
                    {
                        index = i;
                        break;
                    }
                }
                if(index == -1) {
                    index = _f->mergedBones.size();
                    arrayAppend(_f->mergedBones, bone);
                }
                arrayAppend(map, index);
            }
        }
    }
}

void AssimpImporter::doOpenState(const void* state, const Containers::StringView filePath) {
    _f.reset(new File);
    _f->scene = static_cast<const aiScene*>(state);
    _f->filePath.emplace(Containers::String::nullTerminatedGlobalView(filePath));

    doOpenData({}, {});
}

void AssimpImporter::doOpenFile(const Containers::StringView filename) {
    if(!_importer) _importer = createImporter(configuration());

    _f.reset(new File);
    /* Since the slice won't be null terminated, nullTerminatedGlobalView()
       won't help anything here */
    _f->filePath.emplace(Utility::Path::split(filename).first());

    /* File callbacks are set up in doSetFileCallback() */
    if(!(_f->scene = _importer->ReadFile(filename, flagsFromConfiguration(configuration())))) {
        Error{} << "Trade::AssimpImporter::openFile(): failed to open" << filename << Debug::nospace << ":" << _importer->GetErrorString();
        return;
    }

    doOpenData({}, {});
}

void AssimpImporter::doClose() {
    /* In case of doOpenState(), the _importer isn't populated at all and
       the scene is owned by the caller */
    if(_importer) _importer->FreeScene();
    _f.reset();
}

Int AssimpImporter::doDefaultScene() const { return _f->scene->mRootNode ? 0 : -1; }

UnsignedInt AssimpImporter::doSceneCount() const { return _f->scene->mRootNode ? 1 : 0; }

Int AssimpImporter::doSceneForName(const Containers::StringView name) {
    #if ASSIMP_HAS_SCENE_NAME
    if(_f->scene->mRootNode && name == _f->scene->mName)
        return 0;
    #else
    static_cast<void>(name);
    #endif
    return -1;
}

Containers::String AssimpImporter::doSceneName(UnsignedInt) {
    #if ASSIMP_HAS_SCENE_NAME
    return _f->scene->mName;
    #else
    return {};
    #endif
}

Containers::Optional<SceneData> AssimpImporter::doScene(UnsignedInt) {
    /* Count how many meshes and skins we're referencing. Materials have to use
       the same object mapping as meshes and Assimp has no way to not assign a
       material which means there will either be both the mesh and material
       field or neither of the two. */
    UnsignedInt meshCount = 0;
    UnsignedInt skinCount = 0;
    for(const aiNode* const node: _f->nodes) {
        meshCount += node->mNumMeshes;
        for(std::size_t i = 0; i != node->mNumMeshes; ++i)
            if(_f->scene->mMeshes[node->mMeshes[i]]->HasBones())
                ++skinCount;
    }

    /* Count how many objects have cameras and lights. This is stupid because
       Assimp matches node attachments with nodes using a string name, so we
       have to do a stupidly expensive search for every possible camera to see
       if it's referenced by any node.

       This also assumes every camera and light is only referenced by a single
       node. While aiNode docs say that "Cameras and lights reference a
       specific node by name - if there are multiple nodes with this name, they
       are assigned to each of them." but in reality (for COLLADA, at least),
       the camera or light gets duplicated for every node that references it).

       See the cameraLightReferencedByTwoNodes() test, In case this would
       behave differently for some format, with a single camera / light
       actually being referenced by more than one node), the assert for
       lightOffset / cameraOffset at the end will catch this. */
    UnsignedInt cameraCount = 0;
    UnsignedInt lightCount = 0;
    for(std::size_t i = 0; i < _f->scene->mNumCameras; ++i) {
        if(_f->scene->mRootNode->FindNode(_f->scene->mCameras[i]->mName))
            ++cameraCount;
    }
    for(std::size_t i = 0; i < _f->scene->mNumLights; ++i) {
        if(_f->scene->mRootNode->FindNode(_f->scene->mLights[i]->mName))
            ++lightCount;
    }

    /* Allocate the output array.

       The aiNode always has a matrix transformation and instead of checking if
       it's an identity this just assumes that all nodes have a transformation.
       There don't seem to be any TRS attributes, so it's just the matrix. */
    Containers::ArrayView<UnsignedInt> parentImporterStateTransformationObjects;
    Containers::ArrayView<Int> parents;
    Containers::ArrayView<const void*> importerState;
    Containers::ArrayView<Matrix4> transformations;
    Containers::ArrayView<UnsignedInt> meshMaterialObjects;
    Containers::ArrayView<UnsignedInt> meshes;
    Containers::ArrayView<Int> meshMaterials;
    Containers::ArrayView<UnsignedInt> cameraObjects;
    Containers::ArrayView<UnsignedInt> cameras;
    Containers::ArrayView<UnsignedInt> lightObjects;
    Containers::ArrayView<UnsignedInt> lights;
    Containers::ArrayView<UnsignedInt> skinObjects;
    Containers::ArrayView<UnsignedInt> skins;
    Containers::Array<char> data = Containers::ArrayTuple{
        {NoInit, _f->nodes.size(), parentImporterStateTransformationObjects},
        {NoInit, _f->nodes.size(), importerState},
        {NoInit, _f->nodes.size(), parents},
        {NoInit, _f->nodes.size(), transformations},
        {NoInit, meshCount, meshMaterialObjects},
        {NoInit, meshCount, meshes},
        {NoInit, meshCount, meshMaterials},
        {NoInit, cameraCount, cameraObjects},
        {NoInit, cameraCount, cameras},
        {NoInit, lightCount, lightObjects},
        {NoInit, lightCount, lights},
        {NoInit, skinCount, skinObjects},
        {NoInit, skinCount, skins}
    };

    /* Populate the data */
    std::size_t meshMaterialOffset = 0;
    std::size_t cameraOffset = 0;
    std::size_t lightOffset = 0;
    std::size_t skinOffset = 0;
    for(std::size_t i = 0; i != _f->nodes.size(); ++i) {
        const aiNode& node = *_f->nodes[i];

        /* (Implicit) object mapping for the parent, importer state and
           transformation field */
        /** @todo drop this once SceneData supports implicit mapping without
            having to provide the data */
        parentImporterStateTransformationObjects[i] = i;

        /* Populate importer state */
        importerState[i] = &node;

        /* Parent index. Having a parent reference is great because I don't
           have to figure it out of children references.

           In consistency with the distinction in doOpenData(), if the root
           node has children, these are treated as root nodes and the root
           node is ignored; OTOH if there's just the root node alone, then that
           one is treated as a root node. */
        if(!node.mParent || node.mParent == _f->scene->mRootNode) {
            parents[i] = -1;
        } else {
            parents[i] = _f->nodeIndices.at(node.mParent);
        }

        /* Transformation. aiMatrix4x4 is always row-major, transpose. */
        transformations[i] = Matrix4::from(reinterpret_cast<const float*>(&node.mTransformation)).transposed();
        /* Pre-multiply top-level nodes (which are direct children of assimp
           root node) with root node transformation, so things like Y-up/Z-up
           adaptation are preserved. If Assimp gives us only the root node with
           no children, that one is not premultiplied (because that would
           duplicate its own transformation). */
        if(node.mParent == _f->scene->mRootNode)
            transformations[i] = _f->rootTransformation*transformations[i];

        /* Mesh / material references */
        for(std::size_t j = 0; j != node.mNumMeshes; ++j) {
            const UnsignedInt mesh = node.mMeshes[j];
            meshMaterialObjects[meshMaterialOffset] = i;
            meshes[meshMaterialOffset] = mesh;
            meshMaterials[meshMaterialOffset] = _f->scene->mMeshes[mesh]->mMaterialIndex;
            ++meshMaterialOffset;

            Int skin = _f->meshSkins[mesh];
            if(skin != -1) {
                if(_f->mergeSkins) skin = 0;
                skinObjects[skinOffset] = i;
                skins[skinOffset] = skin;
                ++skinOffset;
            }
        }

        /* This insane stupid lookup for cameras and lights again */
        for(std::size_t j = 0; j != _f->scene->mNumCameras; ++j) {
            if(_f->scene->mCameras[j]->mName == node.mName) {
                cameraObjects[cameraOffset] = i;
                cameras[cameraOffset] = j;
                ++cameraOffset;
            }
        }
        for(std::size_t j = 0; j != _f->scene->mNumLights; ++j) {
            if(_f->scene->mLights[j]->mName == node.mName) {
                lightObjects[lightOffset] = i;
                lights[lightOffset] = j;
                ++lightOffset;
            }
        }
    }

    CORRADE_INTERNAL_ASSERT(
        meshMaterialOffset == meshMaterialObjects.size() &&
        lightOffset == lightObjects.size() &&
        cameraOffset == cameraObjects.size() &&
        skinOffset == skinObjects.size());

    /* Put everything together. For simplicity the imported data could always
       have all fields present, with some being empty, but this gives less
       noise for asset introspection purposes. */
    Containers::Array<SceneFieldData> fields;

    /* Parent, ImporterState and Transformation all share the implicit
       object mapping */
    arrayAppend(fields, {
        /** @todo once there's a flag to annotate implicit fields, omit the
            parent field if it's all -1s; or alternatively we could also have a
            stride of 0 for this case */
        SceneFieldData{SceneField::Parent, parentImporterStateTransformationObjects, parents, SceneFieldFlag::ImplicitMapping},
        SceneFieldData{SceneField::ImporterState, parentImporterStateTransformationObjects, importerState, SceneFieldFlag::ImplicitMapping},
        SceneFieldData{SceneField::Transformation, parentImporterStateTransformationObjects, transformations, SceneFieldFlag::ImplicitMapping}
    });

    /* All other fields have the mapping ordered (they get filed as we iterate
       through objects) */
    if(meshCount) arrayAppend(fields, {
        SceneFieldData{SceneField::Mesh, meshMaterialObjects, meshes, SceneFieldFlag::OrderedMapping},
        SceneFieldData{SceneField::MeshMaterial, meshMaterialObjects, meshMaterials, SceneFieldFlag::OrderedMapping},
    });
    if(lightCount) arrayAppend(fields, SceneFieldData{
        SceneField::Light, lightObjects, lights, SceneFieldFlag::OrderedMapping
    });
    if(cameraCount) arrayAppend(fields, SceneFieldData{
        SceneField::Camera, cameraObjects, cameras, SceneFieldFlag::OrderedMapping
    });
    if(skinCount) arrayAppend(fields, SceneFieldData{
        SceneField::Skin, skinObjects, skins, SceneFieldFlag::OrderedMapping
    });

    /* Convert back to the default deleter to avoid dangling deleter function
       pointer issues when unloading the plugin */
    arrayShrink(fields, DefaultInit);
    return SceneData{SceneMappingType::UnsignedInt, _f->nodes.size(), std::move(data), std::move(fields), _f->scene->mRootNode};
}

UnsignedLong AssimpImporter::doObjectCount() const {
    return _f->nodes.size();
}

Long AssimpImporter::doObjectForName(const Containers::StringView name) {
    const aiNode* found = _f->scene->mRootNode->FindNode(aiString(name));
    return found ? Long(_f->nodeIndices.at(found)) : -1;
}

Containers::String AssimpImporter::doObjectName(const UnsignedLong id) {
    return _f->nodes[id]->mName;
}

UnsignedInt AssimpImporter::doCameraCount() const {
    return _f->scene->mNumCameras;
}

Int AssimpImporter::doCameraForName(const Containers::StringView name) {
    if(!_f->camerasForName) {
        _f->camerasForName.emplace();
        _f->camerasForName->reserve(_f->scene->mNumCameras);
        for(std::size_t i = 0; i != _f->scene->mNumCameras; ++i)
            _f->camerasForName->emplace(_f->scene->mCameras[i]->mName, i);
    }

    const auto found = _f->camerasForName->find(name);
    return found == _f->camerasForName->end() ? -1 : found->second;
}

Containers::String AssimpImporter::doCameraName(const UnsignedInt id) {
    return _f->scene->mCameras[id]->mName;
}

Containers::Optional<CameraData> AssimpImporter::doCamera(UnsignedInt id) {
    const aiCamera* cam = _f->scene->mCameras[id];
    const Float aspect = cam->mAspect > 0.0f ? cam->mAspect : 1.0f;
    /** @todo up vector is not used */

    #if ASSIMP_HAS_ORTHOGRAPHIC_CAMERA
    if(cam->mOrthographicWidth > 0.0f)
        return CameraData{CameraType::Orthographic3D,
            Vector2{cam->mOrthographicWidth, cam->mOrthographicWidth/aspect}*2.0f,
            cam->mClipPlaneNear, cam->mClipPlaneFar,
            cam};
    #endif

    return CameraData{CameraType::Perspective3D,
        Rad(cam->mHorizontalFOV), aspect,
        cam->mClipPlaneNear, cam->mClipPlaneFar,
        cam};
}

UnsignedInt AssimpImporter::doLightCount() const {
    return _f->scene->mNumLights;
}

Int AssimpImporter::doLightForName(const Containers::StringView name) {
    if(!_f->lightsForName) {
        _f->lightsForName.emplace();
        _f->lightsForName->reserve(_f->scene->mNumLights);
        for(std::size_t i = 0; i != _f->scene->mNumLights; ++i)
            _f->lightsForName->emplace(_f->scene->mLights[i]->mName, i);
    }

    const auto found = _f->lightsForName->find(name);
    return found == _f->lightsForName->end() ? -1 : found->second;
}

Containers::String AssimpImporter::doLightName(const UnsignedInt id) {
    return _f->scene->mLights[id]->mName;
}

Containers::Optional<LightData> AssimpImporter::doLight(UnsignedInt id) {
    const aiLight* l = _f->scene->mLights[id];

    LightData::Type lightType;
    Color3 color; /** @todo specular color? */
    if(l->mType == aiLightSource_DIRECTIONAL) {
        lightType = LightData::Type::Directional;
        color = Color3{l->mColorDiffuse};
    } else if(l->mType == aiLightSource_POINT) {
        lightType = LightData::Type::Point;
        color = Color3{l->mColorDiffuse};
    } else if(l->mType == aiLightSource_SPOT) {
        lightType = LightData::Type::Spot;
        color = Color3{l->mColorDiffuse};
    } else if(l->mType == aiLightSource_AMBIENT) {
        lightType = LightData::Type::Ambient;
        color = Color3{l->mColorAmbient};
    } else {
        /** @todo area lights */
        Error() << "Trade::AssimpImporter::light(): light type" << l->mType << "is not supported";
        return {};
    }

    /* In case of COLLADA, for a DIRECTIONAL and AMBIENT light this is (1, 0,
       0), which is exactly what we expect (yay). In case of Blend files
       however, this is (1, 0.16, 0.0064) or some other value, so we have to
       patch it to avoid an assert in LightData later. */
    /** @todo for blender the values are calculated from max distance, which
        could be used to restore the `range` property: https://github.com/assimp/assimp/blob/985a5b9667b25390a00e217ee2086882a101d74a/code/AssetLib/Blender/BlenderLoader.cpp#L1251-L1262 */
    Vector3 attenuation{l->mAttenuationConstant, l->mAttenuationLinear, l->mAttenuationQuadratic};
    if((lightType == LightData::Type::Directional || lightType == LightData::Type::Ambient) && attenuation != Vector3{1.0f, 0.0f, 0.0f}) {
        Warning{} << "Trade::AssimpImporter::light(): patching attenuation" << attenuation << "to" << Vector3{1.0f, 0.0f, 0.0f} << "for" << lightType;
        attenuation = {1.0f, 0.0f, 0.0f};
    }

    return LightData{lightType, color, 1.0f, attenuation,
        Rad{l->mAngleInnerCone}, Rad{l->mAngleOuterCone},
        l};
}

UnsignedInt AssimpImporter::doMeshCount() const {
    return _f->scene->mNumMeshes;
}

Int AssimpImporter::doMeshForName(const Containers::StringView name) {
    if(!_f->meshesForName) {
        _f->meshesForName.emplace();
        _f->meshesForName->reserve(_f->scene->mNumMeshes);
        for(std::size_t i = 0; i != _f->scene->mNumMeshes; ++i) {
            _f->meshesForName->emplace(_f->scene->mMeshes[i]->mName, i);
        }
    }

    const auto found = _f->meshesForName->find(name);
    return found == _f->meshesForName->end() ? -1 : found->second;
}

Containers::String AssimpImporter::doMeshName(const UnsignedInt id) {
    return _f->scene->mMeshes[id]->mName;
}

Containers::Optional<MeshData> AssimpImporter::doMesh(const UnsignedInt id, UnsignedInt) {
    const aiMesh* mesh = _f->scene->mMeshes[id];

    /* Primitive. mPrimitiveTypes is a mask but aiProcess_SortByPType (enabled
       by default) should make sure only one type is set. However, since 5.1.0
       triangles can also have aiPrimitiveType_NGONEncodingFlag set which
       indicates that consecutive triangles form an ngon. That flag is always
       set for triangulated meshes. Masking all known types to future-proof
       this against more random flags getting added. */
    /** @todo aiPrimitiveType_NGONEncodingFlag is related to FB_ngon_encoding
        from https://github.com/KhronosGroup/glTF/pull/1620, consider support
        here and in GltfImporter if/when it gets approved */
    MeshPrimitive primitive;
    UnsignedInt expectedFaceSize;
    const aiPrimitiveType primitiveType = aiPrimitiveType(mesh->mPrimitiveTypes &
        (aiPrimitiveType_POINT|aiPrimitiveType_LINE|aiPrimitiveType_TRIANGLE));
    if(primitiveType == aiPrimitiveType_POINT) {
        primitive = MeshPrimitive::Points;
        expectedFaceSize = 1;
    } else if(primitiveType == aiPrimitiveType_LINE) {
        primitive = MeshPrimitive::Lines;
        expectedFaceSize = 2;
    } else if(primitiveType == aiPrimitiveType_TRIANGLE) {
        primitive = MeshPrimitive::Triangles;
        expectedFaceSize = 3;
    } else {
        Error() << "Trade::AssimpImporter::mesh(): unsupported aiPrimitiveType" << mesh->mPrimitiveTypes;
        return Containers::NullOpt;
    }

    /* Gather all attributes. Position is there always, others are optional */
    const UnsignedInt vertexCount = mesh->mNumVertices;
    std::size_t attributeCount = 1;
    std::ptrdiff_t stride = sizeof(Vector3);

    if(mesh->HasNormals()) {
        ++attributeCount;
        stride += sizeof(Vector3);
    }

    /* Assimp provides either none or both, never just one of these */
    if(mesh->HasTangentsAndBitangents()) {
        attributeCount += 2;
        stride += 2*sizeof(Vector3);
    }

    for(std::size_t layer = 0; layer < mesh->GetNumUVChannels(); ++layer) {
        if(mesh->mNumUVComponents[layer] != 2) {
            Warning() << "Trade::AssimpImporter::mesh(): skipping texture coordinate layer" << layer << "which has" << mesh->mNumUVComponents[layer] << "components per coordinate. Only two dimensional texture coordinates are supported.";
            continue;
        }

        ++attributeCount;
        stride += sizeof(Vector2);
    }

    attributeCount += mesh->GetNumColorChannels();
    stride += mesh->GetNumColorChannels()*sizeof(Color4);

    /* Determine the max number of joints assigned to a particular vertex */
    std::size_t maxJointCount = 0;
    /** @todo drop this variable and use just maxJointCount everywhere once
        the compatibilitySkinningAttributes option is gone */
    std::size_t roundedJointCount = 0;
    Containers::Array<UnsignedByte> jointCounts{ValueInit, vertexCount};
    if(mesh->HasBones()) {
        /* Assimp does things the roundabout way of storing per-vertex weights
           in the bones affecting the mesh, we have to undo that */
        for(const aiBone* bone: Containers::arrayView(mesh->mBones, mesh->mNumBones)) {
            if(isDummyBone(bone))
                continue;
            for(const aiVertexWeight& weight: Containers::arrayView(bone->mWeights, bone->mNumWeights)) {
                /* Without IMPORT_NO_SKELETON_MESHES Assimp produces bogus
                   meshes with bones that have invalid vertex ids. We enable
                   that setting but who knows what other rogue settings produce invalid data... */
                CORRADE_ASSERT(weight.mVertexId < vertexCount,
                    "Trade::AssimpImporter::mesh(): invalid vertex id in bone data", {});
                UnsignedByte& jointCount = jointCounts[weight.mVertexId];
                CORRADE_ASSERT(jointCount != std::numeric_limits<UnsignedByte>::max(),
                    "Trade::AssimpImporter::mesh(): too many joint weights", {});
                jointCount++;
                maxJointCount = Math::max<std::size_t>(jointCount, maxJointCount);
            }
        }

        /* Should not get larger than the limit count set in the config */
        #ifndef CORRADE_NO_ASSERT
        const UnsignedInt jointCountLimit = configuration().value<UnsignedInt>("maxJointWeights");
        CORRADE_INTERNAL_ASSERT(jointCountLimit == 0 || maxJointCount <= jointCountLimit);
        #endif

        /* If we supply the compatibility JOINTS and WEIGHTS attributes, pad
           the attributes to a multiple of 4 because users expect those to be
           four-component vectors. Each set of four is then added as an
           additional two attributes. */
        #ifdef MAGNUM_BUILD_DEPRECATED
        if(configuration().value<bool>("compatibilitySkinningAttributes")) {
            roundedJointCount = 4*((maxJointCount + 3)/4);
            attributeCount += 2*(roundedJointCount/4);
        } else
        #endif
        {
            roundedJointCount = maxJointCount;
        }

        attributeCount += 2;
        stride += roundedJointCount*(sizeof(UnsignedInt) + sizeof(Float));
    }

    /* Allocate vertex data, fill in the attributes */
    Containers::Array<char> vertexData{NoInit, std::size_t(stride)*vertexCount};
    Containers::Array<MeshAttributeData> attributeData{attributeCount};
    std::size_t attributeIndex = 0;
    std::size_t attributeOffset = 0;

    /* Positions */
    {
        Containers::StridedArrayView1D<Vector3> positions{vertexData,
            reinterpret_cast<Vector3*>(vertexData + attributeOffset),
            vertexCount, stride};
        Utility::copy(Containers::arrayView(reinterpret_cast<Vector3*>(mesh->mVertices), mesh->mNumVertices), positions);

        attributeData[attributeIndex++] = MeshAttributeData{
            MeshAttribute::Position, positions};
        attributeOffset += sizeof(Vector3);
    }

    /* Normals, if any */
    if(mesh->HasNormals()) {
        Containers::StridedArrayView1D<Vector3> normals{vertexData,
            reinterpret_cast<Vector3*>(vertexData + attributeOffset),
            vertexCount, stride};
        Utility::copy(Containers::arrayView(reinterpret_cast<Vector3*>(mesh->mNormals), mesh->mNumVertices), normals);

        attributeData[attributeIndex++] = MeshAttributeData{
            MeshAttribute::Normal, normals};
        attributeOffset += sizeof(Vector3);
    }

    /* Tangents + bitangents, if any. Assimp always provides either none or
       both, never just one of these. */
    if(mesh->HasTangentsAndBitangents()) {
        Containers::StridedArrayView1D<Vector3> tangents{vertexData,
            reinterpret_cast<Vector3*>(vertexData + attributeOffset),
            vertexCount, stride};
        Utility::copy(Containers::arrayView(reinterpret_cast<Vector3*>(mesh->mTangents), mesh->mNumVertices), tangents);

        attributeData[attributeIndex++] = MeshAttributeData{
            MeshAttribute::Tangent, tangents};
        attributeOffset += sizeof(Vector3);

        Containers::StridedArrayView1D<Vector3> bitangents{vertexData,
            reinterpret_cast<Vector3*>(vertexData + attributeOffset),
            vertexCount, stride};
        Utility::copy(Containers::arrayView(reinterpret_cast<Vector3*>(mesh->mBitangents), mesh->mNumVertices), bitangents);

        attributeData[attributeIndex++] = MeshAttributeData{
            MeshAttribute::Bitangent, bitangents};
        attributeOffset += sizeof(Vector3);
    }

    /* Texture coordinates */
    /** @todo only first uv layer (or "channel") supported) */
    for(std::size_t layer = 0; layer < mesh->GetNumUVChannels(); ++layer) {
        /* Warning already printed above */
        if(mesh->mNumUVComponents[layer] != 2) continue;

        Containers::StridedArrayView1D<Vector2> textureCoordinates{vertexData,
            reinterpret_cast<Vector2*>(vertexData + attributeOffset),
            vertexCount, stride};
        Utility::copy(
            /* Converting to a strided array view to take just the first 2
               component of the 3D coordinate */
            Containers::arrayCast<Vector2>(Containers::stridedArrayView(
                Containers::arrayView(mesh->mTextureCoords[layer], mesh->mNumVertices))),
            textureCoordinates);

        attributeData[attributeIndex++] = MeshAttributeData{
            MeshAttribute::TextureCoordinates, textureCoordinates};
        attributeOffset += sizeof(Vector2);
    }

    /* Colors */
    for(std::size_t layer = 0; layer < mesh->GetNumColorChannels(); ++layer) {
        Containers::StridedArrayView1D<Color4> colors{vertexData,
            reinterpret_cast<Color4*>(vertexData + attributeOffset),
            vertexCount, stride};
        Utility::copy(Containers::arrayView(reinterpret_cast<Color4*>(mesh->mColors[layer]), mesh->mNumVertices), colors);

        attributeData[attributeIndex++] = MeshAttributeData{
            MeshAttribute::Color, colors};
        attributeOffset += sizeof(Color4);
    }

    /* Joints and joint weights */
    if(mesh->HasBones()) {
        Containers::StridedArrayView2D<UnsignedInt> jointIds{vertexData,
            reinterpret_cast<UnsignedInt*>(vertexData + attributeOffset),
            {vertexCount, roundedJointCount}, {stride, sizeof(UnsignedInt)}};
        attributeData[attributeIndex++] = MeshAttributeData{MeshAttribute::JointIds,
            /** @todo drop the prefix() once roundedJointCount is gone */
            jointIds.prefix({vertexCount, maxJointCount})};
        attributeOffset += sizeof(UnsignedInt)*roundedJointCount;
        #ifdef MAGNUM_BUILD_DEPRECATED
        if(configuration().value<bool>("compatibilitySkinningAttributes")) {
            for(std::size_t layer = 0; layer != roundedJointCount; layer += 4)
                attributeData[attributeIndex++] = MeshAttributeData{JointsAttribute, Containers::arrayCast<1, const Vector4ui>(jointIds.sliceSize({0, layer}, {vertexCount, 4}))};
        }
        #endif

        Containers::StridedArrayView2D<Float> weights{vertexData,
            reinterpret_cast<Float*>(vertexData + attributeOffset),
            {vertexCount, roundedJointCount}, {stride, sizeof(Float)}};
        attributeData[attributeIndex++] = MeshAttributeData{MeshAttribute::Weights,
            /** @todo drop the prefix() once roundedJointCount is gone */
            weights.prefix({vertexCount, maxJointCount})};
        attributeOffset += sizeof(Float)*roundedJointCount;
        #ifdef MAGNUM_BUILD_DEPRECATED
        if(configuration().value<bool>("compatibilitySkinningAttributes")) {
            for(std::size_t layer = 0; layer != roundedJointCount; layer += 4)
                attributeData[attributeIndex++] = MeshAttributeData{WeightsAttribute, Containers::arrayCast<1, const Vector4>(weights.sliceSize({0, layer}, {vertexCount, 4}))};
        }
        #endif

        /* Zero-fill, single vertices can have less than the max joint count */
        /** @todo use Utility::fill() once it exists */
        for(std::size_t i = 0; i < vertexCount; ++i) {
            for(std::size_t j = 0; j < roundedJointCount; ++j) {
                jointIds[i][j] = {};
                weights[i][j] = {};
            }
        }

        const Int skin = _f->meshSkins[id];
        CORRADE_INTERNAL_ASSERT(skin != -1);
        /** @todo use Utility::fill() once it exists, especially here where it
            could be a single memset() but due to how AMAZING the STL is, it's
            a stupid loop internally because the 0 is a 32-bit int while
            jointCounts are 8-bit and so a "conversion" has to happen on every
            iteration. */
        std::fill(jointCounts.begin(), jointCounts.end(), 0);

        for(std::size_t b = 0; b != mesh->mNumBones; ++b) {
            const UnsignedInt boneIndex = _f->mergeSkins ? _f->boneMap[skin][b] : b;
            /* Use the original weights, we only need the remapping to patch
               the joint id */
            const aiBone* bone = mesh->mBones[b];
            for(const aiVertexWeight& weight: Containers::arrayView(bone->mWeights, bone->mNumWeights)) {
                if(isDummyBone(bone))
                    continue;
                UnsignedByte& jointCount = jointCounts[weight.mVertexId];
                jointIds[weight.mVertexId][jointCount] = boneIndex;
                weights[weight.mVertexId][jointCount] = weight.mWeight;
                jointCount++;
            }
        }

        /* Assimp glTF 2 importer before version 5.2.5 only read one set of
           joint weight attributes:
           https://github.com/assimp/assimp/commit/a0f375162d48d5a8e70dde2e01f98fa4c5f9b879
           Try to detect such a case and print a warning. We need to check
           < 524 because 5.2.0 to 5.2.4 report their version as 520 but 5.2.5
           is the first to report 524. */
        if(_f->importerIsGltf && maxJointCount == 4 && _f->assimpVersion < 524) {
            for(const Containers::StridedArrayView1D<Float> weightArray: weights) {
                /** @todo sum() in Math/FunctionsBatch.h */
                Float sum = 0.0f;
                for(Float weight: weightArray)
                    sum += weight;

                /* Be very lenient here for shitty exporters. This should still
                   catch most cases of the first set of weights being
                   discarded. */
                constexpr Float Epsilon = 0.1f;
                if(!Math::equal(sum, 0.0f) && Math::abs(1.0f - sum) > Epsilon) {
                    Warning{} <<
                        "Trade::AssimpImporter::mesh(): found non-normalized "
                        "joint weights, possibly a result of Assimp reading "
                        "joint weights incorrectly. Consult the importer "
                        "documentation for more information";
                    break;
                }
            }
        }
    }

    /* Check we pre-calculated well */
    CORRADE_INTERNAL_ASSERT(attributeOffset == std::size_t(stride));
    CORRADE_INTERNAL_ASSERT(attributeIndex == attributeCount);

    /* Import indices. There doesn't seem to be any shortcut to just copy all
       index data in a single go, so having to iterate over faces. Ugh. */
    Containers::Array<char> indexData{NoInit, mesh->mNumFaces*expectedFaceSize*sizeof(UnsignedInt)};
    Containers::ArrayView<UnsignedInt> indices = Containers::arrayCast<UnsignedInt>(indexData);
    for(std::size_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
        const aiFace& face = mesh->mFaces[faceIndex];
        /* For polygons triangulation should have ensured that each face is a
           triangle. */
        CORRADE_ASSERT(face.mNumIndices == expectedFaceSize, "Trade::AssimpImporter::mesh(): expected" << expectedFaceSize << "indices per face for" << primitive << "but got" << face.mNumIndices << "indices for face" << faceIndex, {});

        for(std::size_t i = 0; i != face.mNumIndices; ++i)
            indices[faceIndex*expectedFaceSize + i] = face.mIndices[i];
    }

    return MeshData{primitive,
        std::move(indexData), MeshIndexData{indices},
        std::move(vertexData), std::move(attributeData),
        MeshData::ImplicitVertexCount, mesh};
}

#ifdef MAGNUM_BUILD_DEPRECATED
MeshAttribute AssimpImporter::doMeshAttributeForName(const Containers::StringView name) {
    if(name == "JOINTS"_s) return JointsAttribute;
    if(name == "WEIGHTS"_s) return WeightsAttribute;
    return {};
}

Containers::String AssimpImporter::doMeshAttributeName(UnsignedShort name) {
    if(meshAttributeCustom(name) == JointsAttribute) return "JOINTS"_s;
    if(meshAttributeCustom(name) == WeightsAttribute) return "WEIGHTS"_s;
    return {};
}
#endif

UnsignedInt AssimpImporter::doMaterialCount() const { return _f->scene->mNumMaterials; }

Int AssimpImporter::doMaterialForName(const Containers::StringView name) {
    auto found = _f->materialIndicesForName.find(Containers::String::nullTerminatedView(name));
    return found != _f->materialIndicesForName.end() ? found->second : -1;
}

Containers::String AssimpImporter::doMaterialName(const UnsignedInt id) {
    const aiMaterial* mat = _f->scene->mMaterials[id];
    aiString name;
    mat->Get(AI_MATKEY_NAME, name);

    return name;
}

namespace {

MaterialAttributeData materialColor(MaterialAttribute attribute, const aiMaterialProperty& property) {
    if(property.mDataLength == 4*4)
        return {attribute, MaterialAttributeType::Vector4, property.mData};
    else if(property.mDataLength == 4*3)
        return {attribute, Color4{Color3::from(reinterpret_cast<const Float*>(property.mData))}};
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}
/* We use aiTextureType values to generate names in customMaterialKey(), but
   older Assimp versions may not have those defined. Unfortunately Assimp
   doesn't keep the values stable for us to define our own enum. Instead, this
   SFINAE magic lets us pass through valid values or fall back to unique,
   hopefully unused values otherwise. This way we save ourselves from ifdef-ing
   around specific detected versions. Make sure to double-check any added
   texture type names since they will invariably compile. */
#define ASSIMP_OPTIONAL_TEXTURE_TYPE(name)                                    \
    template<class U> struct AssimpOptionalTextureType_ ## name {             \
        template<class T> constexpr static U get(T*, decltype(T::aiTextureType_ ## name)* = nullptr) { \
            return T::aiTextureType_ ## name;                                 \
        }                                                                     \
        constexpr static U get(...) {                                         \
            return U(1000000 + __LINE__);                                     \
        }                                                                     \
        constexpr static U Value = get(static_cast<U*>(nullptr));             \
    };

ASSIMP_OPTIONAL_TEXTURE_TYPE(BASE_COLOR)
ASSIMP_OPTIONAL_TEXTURE_TYPE(NORMAL_CAMERA)
ASSIMP_OPTIONAL_TEXTURE_TYPE(EMISSION_COLOR)
ASSIMP_OPTIONAL_TEXTURE_TYPE(METALNESS)
ASSIMP_OPTIONAL_TEXTURE_TYPE(DIFFUSE_ROUGHNESS)
ASSIMP_OPTIONAL_TEXTURE_TYPE(AMBIENT_OCCLUSION)
ASSIMP_OPTIONAL_TEXTURE_TYPE(SHEEN)
ASSIMP_OPTIONAL_TEXTURE_TYPE(CLEARCOAT)
ASSIMP_OPTIONAL_TEXTURE_TYPE(TRANSMISSION)

Containers::String customMaterialKey(Containers::StringView key, const aiTextureType semantic) {
    using namespace Containers::Literals;

    /* Create a non-owning key String, add texture type to it if
        present */
    auto keyString = Containers::String::nullTerminatedView(key);
    if(semantic != aiTextureType_NONE) {
        Containers::StringView keyExtra;
        switch(semantic) {
            /* Texture types present in all assimp versions we can reasonably
               support. These are available as far back as 3.2. */
            #define _c(type) case aiTextureType_ ## type: \
                keyExtra = #type ## _s;                   \
                break;
            _c(UNKNOWN)
            _c(AMBIENT)
            _c(DIFFUSE)
            _c(DISPLACEMENT)
            _c(EMISSIVE)
            _c(HEIGHT)
            _c(LIGHTMAP)
            _c(NORMALS)
            _c(OPACITY)
            _c(REFLECTION)
            _c(SHININESS)
            _c(SPECULAR)
            #undef _c

            /* Texture types that may not be available in aiTextureType. Needs
               SFINAE magic to produce unique, unreachable fallback values. */
            #define _f(type) case AssimpOptionalTextureType_ ## type <aiTextureType>::Value: \
                keyExtra = #type ## _s;                                       \
                break;
            #ifdef CORRADE_TARGET_GCC
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wswitch"
            #elif defined(CORRADE_TARGET_MSVC)
            #pragma warning(push)
            #pragma warning(disable: 4063) /* not a valid value for switch of enum 'aiTextureType' */
            #endif
            /* Added in 5.0.0 */
            _f(BASE_COLOR)
            _f(NORMAL_CAMERA)
            _f(EMISSION_COLOR)
            _f(METALNESS)
            _f(DIFFUSE_ROUGHNESS)
            _f(AMBIENT_OCCLUSION)
            /* Added in 5.1.0 */
            _f(SHEEN)
            _f(CLEARCOAT)
            _f(TRANSMISSION)
            #ifdef CORRADE_TARGET_GCC
            #pragma GCC diagnostic pop
            #elif defined(CORRADE_TARGET_MSVC)
            #pragma warning(pop)
            #endif
            #undef _f

            case aiTextureType_NONE:
            case _aiTextureType_Force32Bit:
                CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }

        CORRADE_INTERNAL_ASSERT(!keyExtra.isEmpty());
        keyString = Utility::format("{}.{}", key, keyExtra);
    }

    return keyString;
}

/* MSVC without /Zc:preprocessor (or /experimental:preprocessor) is not able
   to perform deferred macro expansion (which is needed to extract strings out
   of Assimp material macros below), so _CORRADE_HELPER_DEFER isn't even
   defined in that case. Instead, we use an inline function. */
#ifndef _CORRADE_HELPER_DEFER
template<std::size_t size> constexpr Containers::StringView extractMaterialKey(const char(&data)[size], int, int) {
    /* But the suffering doesn't stop there! MSVC 19.32.31329.0 with the
       /permissive- flag says "C3678: string literal after 'operator' must be
       the empty string '""'", and no amount of spaces around the "" helps.
       Without /permissive- or on 19.31.31107.0 and all other versions before
       this worked well; and this works fine in isolation (is the template an
       issue? is the anonymous namespace an issue? questions, so many
       questions). */
    #if defined(CORRADE_TARGET_MSVC) && _MSC_VER < 1932
    return Containers::Literals::operator""_s(data, size - 1);
    #else
    return Containers::StringView{data, size - 1, Containers::StringViewFlag::Global|Containers::StringViewFlag::NullTerminated};
    #endif
}
#endif

}

Containers::Optional<MaterialData> AssimpImporter::doMaterial(const UnsignedInt id) {
    const bool forceRaw = configuration().value<bool>("forceRawMaterialData");
    const bool unrecognizedAsRaw = forceRaw || !configuration().value<bool>("ignoreUnrecognizedMaterialData");

    const aiMaterial* mat = _f->scene->mMaterials[id];

    /* Calculate how many layers there are in the material */
    UnsignedInt maxLayer = 0;
    for(std::size_t i = 0; i != mat->mNumProperties; ++i)
        maxLayer = Math::max(maxLayer, mat->mProperties[i]->mIndex);

    /* Allocate attribute and layer arrays. Only reserve the memory for
       attributes as we'll be skipping properties that don't fit. */
    Containers::Array<MaterialAttributeData> attributes;
    arrayReserve(attributes, mat->mNumProperties);
    Containers::Array<UnsignedInt> layers{maxLayer + 1};

    /* Go through each layer and then for each add all its properties so they
       are consecutive in the array */
    for(UnsignedInt layer = 0; layer <= maxLayer; ++layer) {
        /* Save offset of this layer */
        if(layer != 0) layers[layer - 1] = attributes.size();

        /* Texture indices are consecutive for all textures in the material,
           starting at the offset we saved at the beginning. Because we're
           going layer by layer here, the counting has to be restarted every
           time, and counted also for skipped properties below */
        UnsignedInt textureIndex = _f->textureIndices.at(mat);

        for(std::size_t i = 0; i != mat->mNumProperties; ++i) {
            const aiMaterialProperty& property = *mat->mProperties[i];

            /* Process only properties from this layer (again, to have them
               consecutive in the attribute array), but properly increase
               texture index even for the skipped properties so we have the
               mapping correct */
            if(property.mIndex != layer) {
                if(Containers::StringView{property.mKey} == _AI_MATKEY_TEXTURE_BASE)
                    ++textureIndex;
                continue;
            }

            using namespace Containers::Literals;

            const Containers::StringView key{property.mKey};

            /* AI_MATKEY_* are in form "bla",0,0, so extract the first part
               and turn it into a StringView for string comparison. The
               _s literal is there to avoid useless strlen() calls in every
               branch. _CORRADE_HELPER_DEFER is not implementable on MSVC
               (see docs in Utility/Macros.h), so there it's a constexpr
               function instead. */
            #ifdef _CORRADE_HELPER_DEFER
            #define _str2(name, i, j) name ## _s
            #define _str(name) _CORRADE_HELPER_DEFER(_str2, name)
            #else
            #define _str extractMaterialKey
            #endif

            /* Material name is available through materialName() /
               materialForName() already, ignore it */
            if(key == _str(AI_MATKEY_NAME) && property.mType == aiPTI_String)
                continue;

            /* Recognize known attributes if they have expected types and
               sizes. If they don't, they'll be treated as custom attribs in
               the code below. It's also possible to skip this by enabling the
               forceRawMaterialData configuration option. */
            MaterialAttributeData data;
            MaterialAttribute attribute{};
            MaterialAttributeType type{};
            if(!forceRaw) {
                /* Properties not tied to a particular texture */
                if(property.mSemantic == aiTextureType_NONE) {
                    /* Colors. Some formats have them three-components (OBJ),
                       some four-component (glTF). Documentation states it's
                       always three-component. FFS. */
                    if(key == _str(AI_MATKEY_COLOR_AMBIENT) && property.mType == aiPTI_Float && (property.mDataLength == 4*4 || property.mDataLength == 4*3)) {
                        data = materialColor(MaterialAttribute::AmbientColor, property);

                        /* Assimp 4.1 forces ambient color to white for STL
                           models. That's just plain wrong, so we force it back
                           to black (and emit a warning, so in the very rare
                           case when someone would actually want white ambient,
                           they'll know it got overriden). Fixed by
                           https://github.com/assimp/assimp/pull/2563 in 5.0.

                           In addition, we abuse this fix in case Assimp
                           imports ambient textures as LIGHTMAP. Those are not
                           recognized right now (because WHY THE FUCK one would
                           import an ambient texture as something else?!) and
                           so the ambient color, which is white in this case as
                           well, makes no sense. */
                        aiString texturePath;
                        if(configuration().value<bool>("forceWhiteAmbientToBlack") && data.value<Color4>() == Color4{1.0f} && mat->Get(AI_MATKEY_TEXTURE(aiTextureType_AMBIENT, layer), texturePath) != AI_SUCCESS) {
                            Warning{} << "Trade::AssimpImporter::material(): white ambient detected, forcing back to black";
                            data = {MaterialAttribute::AmbientColor, Color4{0.0f, 1.0f}};
                        }

                    } else if(key == _str(AI_MATKEY_COLOR_DIFFUSE) && property.mType == aiPTI_Float && (property.mDataLength == 4*4 || property.mDataLength == 4*3)) {
                        data = materialColor(MaterialAttribute::DiffuseColor, property);
                    } else if(key == _str(AI_MATKEY_COLOR_SPECULAR) && property.mType == aiPTI_Float && (property.mDataLength == 4*4 || property.mDataLength == 4*3)) {
                        data = materialColor(MaterialAttribute::SpecularColor, property);

                    /* Factors */
                    } else if(key == _str(AI_MATKEY_SHININESS) && property.mType == aiPTI_Float && property.mDataLength == 4) {
                        attribute = MaterialAttribute::Shininess;
                        type = MaterialAttributeType::Float;
                    }

                    /** @todo
                        - MaterialAttribute::DoubleSided - AI_MATKEY_TWOSIDED
                        - MaterialAttribute::AlphaBlend - AI_MATKEY_GLTF_ALPHAMODE == "BLEND", glTF only
                        - MaterialAttribute::AlphaMask - AI_MATKEY_GLTF_ALPHAMODE == "MASK" + AI_MATKEY_GLTF_ALPHACUTOFF, glTF only
                        - MaterialType::Flat - AI_MATKEY_SHADING_MODEL + aiShadingMode_NoShading.
                          For versions < 5.1.0 this is AI_MATKEY_GLTF_UNLIT for glTF. */

                /* Properties tied to a particular texture */
                } else {
                    /* Texture index */
                    if(key == _AI_MATKEY_TEXTURE_BASE) {
                        switch(property.mSemantic) {
                            case aiTextureType_AMBIENT:
                                attribute = MaterialAttribute::AmbientTexture;
                                break;
                            case aiTextureType_DIFFUSE:
                                attribute = MaterialAttribute::DiffuseTexture;
                                break;
                            case aiTextureType_SPECULAR:
                                attribute = MaterialAttribute::SpecularTexture;
                                break;
                            case aiTextureType_NORMALS:
                                attribute = MaterialAttribute::NormalTexture;
                                break;
                        }

                        /* Save only if the name is recognized (and let it
                           be imported as a custom attribute otherwise),
                           but increment the texture index counter always
                           to stay in sync */
                        if(attribute != MaterialAttribute{})
                            data = {attribute, textureIndex};
                        ++textureIndex;

                    /* Texture matrix */
                    } else if(key == _AI_MATKEY_UVTRANSFORM_BASE && property.mType == aiPTI_Float && property.mDataLength == 20) {
                        switch(property.mSemantic) {
                            case aiTextureType_AMBIENT:
                                attribute = MaterialAttribute::AmbientTextureMatrix;
                                break;
                            case aiTextureType_DIFFUSE:
                                attribute = MaterialAttribute::DiffuseTextureMatrix;
                                break;
                            case aiTextureType_SPECULAR:
                                attribute = MaterialAttribute::SpecularTextureMatrix;
                                break;
                            case aiTextureType_NORMALS:
                                attribute = MaterialAttribute::NormalTextureMatrix;
                                break;
                        }

                        /* Save only if the name is recognized (and let it
                           be imported as a custom attribute otherwise) */
                        if(attribute != MaterialAttribute{}) {
                            const auto& transform = *reinterpret_cast<const aiUVTransform*>(property.mData);
                            data = {attribute,
                                Matrix3::translation(Vector2{transform.mTranslation})*
                                /* The rotation is said to be around (0.5, 0.5)
                                   so we translate, rotate, and translate back.
                                   It's also said to be counterclockwise in the
                                   aiUVTransform docs, but at least the glTF
                                   importer makes it clockwise, judging from
                                   the output for the TextureTransformTest
                                   glTF model. The rotation probably isn't
                                   really correct either, see the
                                   materialTextureMatrix() test for further
                                   details. */
                                Matrix3::translation(Vector2{0.5f})*
                                Matrix3::rotation(Rad{-transform.mRotation})*
                                Matrix3::translation(Vector2{-0.5f})*
                                Matrix3::scaling(Vector2{transform.mScaling})};
                        }

                    /* Texture coordinate set index */
                    } else if(key == _AI_MATKEY_UVWSRC_BASE && property.mType == aiPTI_Integer && property.mDataLength == 4) {
                        type = MaterialAttributeType::UnsignedInt;
                        switch(property.mSemantic) {
                            case aiTextureType_AMBIENT:
                                attribute = MaterialAttribute::AmbientTextureCoordinates;
                                break;
                            case aiTextureType_DIFFUSE:
                                attribute = MaterialAttribute::DiffuseTextureCoordinates;
                                break;
                            case aiTextureType_SPECULAR:
                                attribute = MaterialAttribute::SpecularTextureCoordinates;
                                break;
                            case aiTextureType_NORMALS:
                                attribute = MaterialAttribute::NormalTextureCoordinates;
                                break;
                        }
                    }
                }

                /** @todo PBR support. Assimp 5.1.0 finally has unified support
                    for PBR attributes, including the fancy glTF extensions:
                    https://github.com/assimp/assimp/blob/v5.1.0/include/assimp/material.h#L971 */
            }

            #undef _str
            #undef _str2

            /* If the attribute data is already constructed (parsed from a
               string value etc), put it directly in */
            if(data.type() != MaterialAttributeType{}) {
                arrayAppend(attributes, data);

            /* Otherwise, if we know the name and type, use mData for the value */
            } else if(attribute != MaterialAttribute{}) {
                /* For string attributes we'd need to pass StringView instead
                   of mData, but there are none so far so assert for now */
                CORRADE_INTERNAL_ASSERT(type != MaterialAttributeType{} && type != MaterialAttributeType::String);
                arrayAppend(attributes, InPlaceInit, attribute, type, property.mData);

            /* Otherwise figure out its type, do some additional checking and
               put it there as a custom attribute */
            } else if(unrecognizedAsRaw) {
                /* Attribute names starting with uppercase letters are reserved
                   for Magnum. All assimp material keys start with one of $/?/~
                   so an assert should be fine here:
                   https://github.com/assimp/assimp/blob/v3.2/include/assimp/material.h#L548
                   Revisit if this breaks for someone. */
                CORRADE_ASSERT(!key.isEmpty() && !std::isupper(key.front()),
                    "Trade::AssimpImporter::material(): attribute names starting with uppercase are reserved:" << key, {});

                if(property.mType == aiPTI_Integer) {
                    if(property.mDataLength == 1)
                        type = MaterialAttributeType::Bool;
                    /* Don't compare mDataLength/4 because we can never be sure
                       the value is actually a multiple of four */
                    else if(property.mDataLength == 4)
                        type = MaterialAttributeType::Int;
                    else if(property.mDataLength == 8)
                        type = MaterialAttributeType::Vector2i;
                    else if(property.mDataLength == 12)
                        type = MaterialAttributeType::Vector3i;
                    else if(property.mDataLength == 16)
                        type = MaterialAttributeType::Vector4i;
                    else {
                        Warning{} << "Trade::AssimpImporter::material(): property" << key << "is an integer array of" << property.mDataLength << "bytes, saving as a typeless buffer";
                        /* Abusing Pointer to indicate this is a buffer.
                           Together with other similar cases it's processed
                           below and turned into MaterialAttributeType::Buffer. */
                        type = MaterialAttributeType::Pointer;
                    }
                } else if(property.mType == aiPTI_Float) {
                    /* Don't compare mDataLength/4 because we can never be sure
                       the value is actually a multiple of four */
                    if(property.mDataLength == 4)
                        type = MaterialAttributeType::Float;
                    else if(property.mDataLength == 8)
                        type = MaterialAttributeType::Vector2;
                    else if(property.mDataLength == 12)
                        type = MaterialAttributeType::Vector3;
                    else if(property.mDataLength == 16)
                        type = MaterialAttributeType::Vector4;
                    else {
                        Warning{} << "Trade::AssimpImporter::material(): property" << key << "is a float array of" << property.mDataLength << "bytes, saving as a typeless buffer";
                        type = MaterialAttributeType::Pointer;
                    }
                }
                #if ASSIMP_HAS_DOUBLES
                else if(property.mType == aiPTI_Double) {
                    /** @todo This shouldn't happen without compiling Assimp
                        with double support. But then the importer would only
                        produce garbage because it assumes ai_real equals float
                        everywhere.
                        Always assert? Ignore if ASSIMP_DOUBLE_PRECISION is
                        not defined? */
                    Warning{} << "Trade::AssimpImporter::material():" << key << "is a double precision property, saving as a typeless buffer";
                    type = MaterialAttributeType::Pointer;
                }
                #endif
                else if(property.mType == aiPTI_Buffer) {
                    type = MaterialAttributeType::Pointer;
                } else if(property.mType == aiPTI_String) {
                    type = MaterialAttributeType::String;
                } else {
                    Warning{} << "Trade::AssimpImporter::material(): property" << key << "has unknown type" << property.mType << Debug::nospace << ", saving as a typeless buffer";
                    type = MaterialAttributeType::Pointer;
                }

                CORRADE_INTERNAL_ASSERT(type != MaterialAttributeType{});

                Containers::StringView valueString;
                Containers::ArrayView<const void> valueBuffer;
                std::size_t valueSize;
                const void* valuePointer;
                if(type == MaterialAttributeType::Pointer) {
                    /* Typeless buffer, turn it into an owned Buffer */
                    type = MaterialAttributeType::Buffer;
                    valueBuffer = {property.mData, property.mDataLength};
                    /* +1 is size */
                    valueSize = property.mDataLength + 1;
                    valuePointer = &valueBuffer;
                } else if(type == MaterialAttributeType::String) {
                    valueString = materialPropertyString(property);
                    /* +2 is null byte + size */
                    valueSize = valueString.size() + 2;
                    valuePointer = &valueString;
                } else {
                    valueSize = materialAttributeTypeSize(type);
                    valuePointer = property.mData;
                }

                CORRADE_INTERNAL_ASSERT(type != MaterialAttributeType::Pointer &&
                                        type != MaterialAttributeType::MutablePointer);

                const Containers::String keyString = customMaterialKey(key, aiTextureType(property.mSemantic));

                /* +1 is null byte for the key */
                if(valueSize + keyString.size() + 1 + sizeof(MaterialAttributeType) > sizeof(MaterialAttributeData)) {
                    Warning{} << "Trade::AssimpImporter::material(): property" << keyString <<
                        "is too large with" << valueSize + keyString.size() << "bytes, skipping";
                    continue;
                }

                arrayAppend(attributes, InPlaceInit, keyString, type, valuePointer);
            }
        }
    }

    /* Save offset for the last layer */
    layers.back() = attributes.size();

    /* Can't use growable deleters in a plugin, convert back to the default
       deleter */
    arrayShrink(attributes, DefaultInit);

    /** @todo detect PBR properties and add relevant types accordingly */
    const MaterialType materialType = forceRaw ? MaterialType{} : MaterialType::Phong;
    return MaterialData{materialType, std::move(attributes), std::move(layers), mat};
}

UnsignedInt AssimpImporter::doTextureCount() const { return _f->textures.size(); }

Containers::Optional<TextureData> AssimpImporter::doTexture(const UnsignedInt id) {
    auto toWrapping = [](aiTextureMapMode mapMode) {
        switch (mapMode) {
            case aiTextureMapMode_Wrap:
                return SamplerWrapping::Repeat;
            case aiTextureMapMode_Decal:
                Warning() << "Trade::AssimpImporter::texture(): no wrapping "
                    "enum to match aiTextureMapMode_Decal, using "
                    "SamplerWrapping::ClampToEdge";
                return SamplerWrapping::ClampToEdge;
            case aiTextureMapMode_Clamp:
                return SamplerWrapping::ClampToEdge;
            case aiTextureMapMode_Mirror:
                return SamplerWrapping::MirroredRepeat;
            default:
                Warning() << "Trade::AssimpImporter::texture(): unknown "
                    "aiTextureMapMode" << mapMode << Debug::nospace << ", "
                    "using SamplerWrapping::ClampToEdge";
                return SamplerWrapping::ClampToEdge;
        }
    };

    aiTextureMapMode mapMode;
    const aiMaterial* mat = _f->textures[id].first();
    const aiTextureType type = aiTextureType(mat->mProperties[_f->textures[id].second()]->mSemantic);
    SamplerWrapping wrappingU = SamplerWrapping::ClampToEdge;
    SamplerWrapping wrappingV = SamplerWrapping::ClampToEdge;
    if(mat->Get(AI_MATKEY_MAPPINGMODE_U(type, 0), mapMode) == AI_SUCCESS)
        wrappingU = toWrapping(mapMode);
    if(mat->Get(AI_MATKEY_MAPPINGMODE_V(type, 0), mapMode) == AI_SUCCESS)
        wrappingV = toWrapping(mapMode);

    /** @todo AI_MATKEY_GLTF_MAPPINGFILTER_{MIN, MAG} for glTF, is there
        anything else? */

    return TextureData{TextureType::Texture2D,
        SamplerFilter::Linear, SamplerFilter::Linear, SamplerMipmap::Linear,
        {wrappingU, wrappingV, SamplerWrapping::ClampToEdge}, _f->textures[id].third(), &_f->textures[id]};
}

UnsignedInt AssimpImporter::doImage2DCount() const { return _f->images.size(); }

AbstractImporter* AssimpImporter::setupOrReuseImporterForImage(const UnsignedInt id, const char* const errorPrefix) {
    const aiMaterial* mat = _f->images[id].first();
    const aiTextureType type = aiTextureType(mat->mProperties[_f->images[id].second()]->mSemantic);

    /* Looking for the same ID, so reuse an importer populated before. If the
       previous attempt failed, the importer is not set, so return nullptr in
       that case. Going through everything below again would not change the
       outcome anyway, only spam the output with redundant messages. */
    if(_f->imageImporterId == id)
        return _f->imageImporter ? &*_f->imageImporter : nullptr;

    /* Otherwise reset the importer and remember the new ID. If the import
       fails, the importer will stay unset, but the ID will be updated so the
       next round can again just return nullptr above instead of going through
       the doomed-to-fail process again. */
    _f->imageImporter = Containers::NullOpt;
    _f->imageImporterId = id;

    aiString texturePath;
    if(mat->Get(AI_MATKEY_TEXTURE(type, 0), texturePath) != AI_SUCCESS) {
        Error{} << errorPrefix << "error getting path for texture" << id;
        return {};
    }

    AnyImageImporter importer{*manager()};
    importer.setFlags(flags());
    if(fileCallback()) importer.setFileCallback(fileCallback(), fileCallbackUserData());

    const Containers::StringView path = texturePath;

    /* Loading of embedded textures was changed to a lookup using the full path
       embedded with the scene file rather than an index prefixed with '*' in
       Assimp commit 4623c2f14c121e4792e74169b6cef09631a4184a (released with
       5.0.0). */
    #if ASSIMP_IS_VERSION_5_OR_GREATER
    /* If loading the texture as embedded fails, it must be external */
    const aiTexture* texture = _f->scene->GetEmbeddedTexture(path.data());
    #else
    /* If path is prefixed with '*', load embedded texture */
    const aiTexture* texture = nullptr;
    if(path.hasPrefix('*')) {
        char* err;
        const Containers::StringView indexStr = path.exceptPrefix(1);
        const char* str = indexStr.data();
        const Int index = Int(std::strtol(str, &err, 10));
        if(err == nullptr || err == str) {
            Error{} << errorPrefix << "embedded texture path did not contain a valid integer string";
            return nullptr;
        }

        texture = _f->scene->mTextures[index];
    }
    #endif
    if(texture) {
        if(texture->mHeight == 0) {
            /* Compressed image data */
            auto textureData = Containers::ArrayView<const char>(reinterpret_cast<const char*>(texture->pcData), texture->mWidth);

            if(!importer.openData(textureData))
                return nullptr;

        /* Uncompressed image data */
        } else {
            Error{} << errorPrefix << "uncompressed embedded image data is not supported";
            return nullptr;
        }

    /* Load external texture */
    } else {
        if(!_f->filePath && !fileCallback()) {
            Error{} << errorPrefix << "external images can be imported only when opening files from the filesystem or if a file callback is present";
            return nullptr;
        }

        /* Assimp loads image path references as-is. It might contain windows path separators if the exporter didn't normalize */
        Containers::String normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        /* Assimp doesn't trim spaces from the end of image paths in OBJ
           materials so we have to. See the image-filename-space.mtl test. */
        if(!importer.openFile(Utility::Path::join(_f->filePath ? *_f->filePath : "", normalized).trimmed()))
            return nullptr;
    }

    if(importer.image2DCount() != 1) {
        Error{} << errorPrefix << "expected exactly one 2D image in an image file but got" << importer.image2DCount();
        return nullptr;
    }

    return &_f->imageImporter.emplace(std::move(importer));
}

UnsignedInt AssimpImporter::doImage2DLevelCount(const UnsignedInt id) {
    CORRADE_ASSERT(manager(), "Trade::AssimpImporter::image2DLevelCount(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    AbstractImporter* importer = setupOrReuseImporterForImage(id, "Trade::AssimpImporter::image2DLevelCount():");
    /* image2DLevelCount() isn't supposed to fail (image2D() is, instead), so
       report 1 on failure and expect image2D() to fail later */
    if(!importer) return 1;

    return importer->image2DLevelCount(0);
}

Containers::Optional<ImageData2D> AssimpImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    CORRADE_ASSERT(manager(), "Trade::AssimpImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    AbstractImporter* importer = setupOrReuseImporterForImage(id, "Trade::AssimpImporter::image2D():");
    if(!importer) return Containers::NullOpt;

    return importer->image2D(0, level);
}

UnsignedInt AssimpImporter::doAnimationCount() const {
    /* If the animations are merged, there's at most one */
    if(configuration().value<bool>("mergeAnimationClips"))
        return _f->scene->mNumAnimations ? 1 : 0;

    return _f->scene->mNumAnimations;
}

Int AssimpImporter::doAnimationForName(const Containers::StringView name) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return -1;

    if(!_f->animationsForName) {
        _f->animationsForName.emplace();
        _f->animationsForName->reserve(_f->scene->mNumAnimations);
        for(std::size_t i = 0; i != _f->scene->mNumAnimations; ++i)
            _f->animationsForName->emplace(_f->scene->mAnimations[i]->mName, i);
    }

    const auto found = _f->animationsForName->find(name);
    return found == _f->animationsForName->end() ? -1 : found->second;
}

Containers::String AssimpImporter::doAnimationName(UnsignedInt id) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return {};
    return _f->scene->mAnimations[id]->mName;
}

namespace {

Animation::Extrapolation extrapolationFor(aiAnimBehaviour behaviour) {
    /* This code is not covered by tests since there is currently
       no code in Assimp that sets this except for the .irr importer.
       So it'll be aiAnimBehaviour_DEFAULT for 99.99% of users,
       and there are tests that check that this becomes
       Extrapolation::Constant. */
    switch(behaviour) {
        case aiAnimBehaviour_DEFAULT:
            /** @todo emulate this correctly by inserting keyframes
                with the default node transformation */
            return Animation::Extrapolation::Constant;
        case aiAnimBehaviour_CONSTANT:
            return Animation::Extrapolation::Constant;
        case aiAnimBehaviour_LINEAR:
            return Animation::Extrapolation::Extrapolated;
        case aiAnimBehaviour_REPEAT:
            return Animation::Extrapolation::Constant;
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }
}

}

Containers::Optional<AnimationData> AssimpImporter::doAnimation(UnsignedInt id) {
    const bool verbose{flags() & ImporterFlag::Verbose};
    const bool removeDummyAnimationTracks = configuration().value<bool>("removeDummyAnimationTracks");
    const bool mergeAnimationClips = configuration().value<bool>("mergeAnimationClips");

    /* Import either a single animation or all of them together. At the moment,
       Blender doesn't really support cinematic animations (affecting multiple
       objects): https://blender.stackexchange.com/q/5689. And since
       https://github.com/KhronosGroup/glTF-Blender-Exporter/pull/166, these
       are exported as a set of object-specific clips, which may not be wanted,
       so we give the users an option to merge them all together. */
    const std::size_t animationBegin = mergeAnimationClips ? 0 : id;
    const std::size_t animationEnd = mergeAnimationClips ? _f->scene->mNumAnimations : id + 1;

    /* Calculate total channel count */
    std::size_t channelCount = 0;
    for(std::size_t a = animationBegin; a != animationEnd; ++a) {
        channelCount += _f->scene->mAnimations[a]->mNumChannels;
    }

    /* Presence of translation/rotation/scaling keys for each channel. We might
       skip certain tracks (see comments in the next loop) but we need to know
       the correct track count and data array size up front, so determine this
       and remember it for the actual loop that extracts the tracks. BigEnumSet
       because EnumSet requires binary-exclusive enum values. */
    typedef Containers::BigEnumSet<AnimationTrackTargetType> TargetTypes;
    Containers::Array<TargetTypes> channelTargetTypes{channelCount};

    /* Calculate total data size and track count. If merging all animations
       together, this is the sum of all clip track counts. */
    std::size_t dataSize = 0;
    std::size_t trackCount = 0;
    std::size_t currentChannel = 0;
    for(std::size_t a = animationBegin; a != animationEnd; ++a) {
        const aiAnimation* animation = _f->scene->mAnimations[a];
        for(std::size_t c = 0; c != animation->mNumChannels; ++c) {
            const aiNodeAnim* channel = animation->mChannels[c];

            UnsignedInt translationKeyCount = channel->mNumPositionKeys;
            UnsignedInt rotationKeyCount = channel->mNumRotationKeys;
            UnsignedInt scalingKeyCount = channel->mNumScalingKeys;

            /* Assimp adds useless 1-key tracks with default node translation/
               rotation/scale if the channel doesn't target all 3 of them.
               Ignore those. */
            if(removeDummyAnimationTracks &&
                (translationKeyCount == 1 || rotationKeyCount == 1 || scalingKeyCount == 1))
            {
                const aiNode* node = _f->scene->mRootNode->FindNode(channel->mNodeName);
                CORRADE_INTERNAL_ASSERT(node);

                aiVector3D nodeTranslation;
                aiQuaternion nodeRotation;
                aiVector3D nodeScaling;
                /* This might not perfectly restore the T/R/S components, but
                   it's okay if the comparison fails, this whole fix is a
                   best-effort attempt */
                node->mTransformation.Decompose(nodeScaling, nodeRotation, nodeTranslation);

                if(translationKeyCount == 1 && channel->mPositionKeys[0].mTime == 0.0) {
                    const Vector3 value = Vector3::from(&channel->mPositionKeys[0].mValue[0]);
                    const Vector3 nodeValue = Vector3::from(&nodeTranslation[0]);
                    if(value == nodeValue) {
                        if(verbose) Debug{}
                            << "Trade::AssimpImporter::animation(): ignoring "
                               "dummy translation track in animation"
                            << a << Debug::nospace << ", channel" << c;
                        translationKeyCount = 0;
                    }
                }
                if(rotationKeyCount == 1 && channel->mRotationKeys[0].mTime == 0.0) {
                    const aiQuaternion& valueQuat = channel->mRotationKeys[0].mValue;
                    const Quaternion value{{valueQuat.x, valueQuat.y, valueQuat.z}, valueQuat.w};
                    const aiQuaternion& nodeQuat = nodeRotation;
                    const Quaternion nodeValue{{nodeQuat.x, nodeQuat.y, nodeQuat.z}, nodeQuat.w};
                    if(value == nodeValue) {
                        if(verbose) Debug{}
                            << "Trade::AssimpImporter::animation(): ignoring "
                               "dummy rotation track in animation"
                            << a << Debug::nospace << ", channel" << c;
                        rotationKeyCount = 0;
                    }
                }
                if(scalingKeyCount == 1 && channel->mScalingKeys[0].mTime == 0.0) {
                    const Vector3 value = Vector3::from(&channel->mScalingKeys[0].mValue[0]);
                    const Vector3 nodeValue = Vector3::from(&nodeScaling[0]);
                    if(value == nodeValue) {
                        if(verbose) Debug{}
                            << "Trade::AssimpImporter::animation(): ignoring "
                               "dummy scaling track in animation"
                            << a << Debug::nospace << ", channel" << c;
                        scalingKeyCount = 0;
                    }
                }
            }

            TargetTypes targetTypes;
            if(translationKeyCount > 0)
                targetTypes |= AnimationTrackTargetType::Translation3D;
            if(rotationKeyCount > 0)
                targetTypes |= AnimationTrackTargetType::Rotation3D;
            if(scalingKeyCount > 0)
                targetTypes |= AnimationTrackTargetType::Scaling3D;
            channelTargetTypes[currentChannel++] = targetTypes;

            /** @todo handle alignment once we do more than just four-byte types */
            dataSize += translationKeyCount*(sizeof(Float) + sizeof(Vector3)) +
                rotationKeyCount*(sizeof(Float) + sizeof(Quaternion)) +
                scalingKeyCount*(sizeof(Float) + sizeof(Vector3));

            trackCount += (translationKeyCount > 0 ? 1 : 0) +
                (rotationKeyCount > 0 ? 1 : 0) +
                (scalingKeyCount > 0 ? 1 : 0);
        }
    }

    /* Populate the data array */
    Containers::Array<char> data{dataSize};

    const bool optimizeQuaternionShortestPath = configuration().value<bool>("optimizeQuaternionShortestPath");
    const bool normalizeQuaternions = configuration().value<bool>("normalizeQuaternions");

    /* Import all tracks */
    bool hadToRenormalize = false;
    std::size_t dataOffset = 0;
    std::size_t trackId = 0;
    currentChannel = 0;
    Containers::Array<Trade::AnimationTrackData> tracks{trackCount};
    for(std::size_t a = animationBegin; a != animationEnd; ++a) {
        const aiAnimation* animation = _f->scene->mAnimations[a];
        for(std::size_t c = 0; c != animation->mNumChannels; ++c) {
            const aiNodeAnim* channel = animation->mChannels[c];
            const aiNode* node = CORRADE_INTERNAL_ASSERT_EXPRESSION(_f->scene->mRootNode->FindNode(channel->mNodeName));
            const Int target = _f->nodeIndices.at(node);

            /* Assimp only supports linear interpolation. For glTF splines
               it simply uses the spline control points. */
            constexpr Animation::Interpolation interpolation = Animation::Interpolation::Linear;

            /* Extrapolation modes */
            const Animation::Extrapolation extrapolationBefore = extrapolationFor(channel->mPreState);
            const Animation::Extrapolation extrapolationAfter = extrapolationFor(channel->mPostState);

            /* Key times are given as ticks, convert to milliseconds. Default
               value of 25 is taken from the assimp_view tool. */
            double ticksPerSecond = animation->mTicksPerSecond != 0.0 ? animation->mTicksPerSecond : 25.0;

            /* For glTF files mTicksPerSecond is completely useless before
               https://github.com/assimp/assimp/commit/09d80ff478d825a80bce6fb787e8b19df9f321a8
               but can be assumed to always be 1000.

               FBX files reported incorrect mTicksPerSecond in versions 5.1.0
               to 5.1.3, but the fix in 5.1.4 broke the detection and
               workaround we had so we no longer patch anything for these
               versions. https://github.com/assimp/assimp/issues/4197 */
            constexpr Double CorrectTicksPerSecond = 1000.0;
            if(_f->importerIsGltf && !Math::equal(ticksPerSecond, CorrectTicksPerSecond)) {
                if(verbose) Debug{}
                    << "Trade::AssimpImporter::animation():" << Float(ticksPerSecond)
                    << "ticks per second is incorrect for glTF, patching to"
                    << Float(CorrectTicksPerSecond);
                ticksPerSecond = CorrectTicksPerSecond;
            }

            const TargetTypes targetTypes = channelTargetTypes[currentChannel++];

            /* Translation */
            if(targetTypes & AnimationTrackTargetType::Translation3D) {
                const size_t keyCount = channel->mNumPositionKeys;
                const auto keys = Containers::arrayCast<Float>(
                    data.sliceSize(dataOffset, keyCount*sizeof(Float)));
                dataOffset += keys.size()*sizeof(keys[0]);
                const auto values = Containers::arrayCast<Vector3>(
                    data.sliceSize(dataOffset, keyCount*sizeof(Vector3)));
                dataOffset += values.size()*sizeof(values[0]);

                for(size_t k = 0; k < channel->mNumPositionKeys; ++k) {
                    /* Convert double to float keys */
                    keys[k] = channel->mPositionKeys[k].mTime / ticksPerSecond;
                    values[k] = Vector3::from(&channel->mPositionKeys[k].mValue[0]);
                }

                const auto track = Animation::TrackView<const Float, const Vector3>{
                    keys, values, interpolation,
                    animationInterpolatorFor<Vector3>(interpolation),
                    extrapolationBefore, extrapolationAfter};

                tracks[trackId++] = AnimationTrackData{AnimationTrackType::Vector3,
                    AnimationTrackType::Vector3, AnimationTrackTargetType::Translation3D,
                    static_cast<UnsignedInt>(target), track };
            }

            /* Rotation */
            if(targetTypes & AnimationTrackTargetType::Rotation3D) {
                const size_t keyCount = channel->mNumRotationKeys;
                const auto keys = Containers::arrayCast<Float>(
                    data.sliceSize(dataOffset, keyCount*sizeof(Float)));
                dataOffset += keys.size()*sizeof(keys[0]);
                const auto values = Containers::arrayCast<Quaternion>(
                    data.sliceSize(dataOffset, keyCount*sizeof(Quaternion)));
                dataOffset += values.size()*sizeof(values[0]);

                for(size_t k = 0; k < channel->mNumRotationKeys; ++k) {
                    keys[k] = channel->mRotationKeys[k].mTime / ticksPerSecond;
                    const aiQuaternion& quat = channel->mRotationKeys[k].mValue;
                    values[k] = Quaternion{{quat.x, quat.y, quat.z}, quat.w};
                }

                /* Ensure shortest path is always chosen. */
                if(optimizeQuaternionShortestPath) {
                    Float flip = 1.0f;
                    for(std::size_t i = 0; i + 1 < values.size(); ++i) {
                        if(Math::dot(values[i], values[i + 1]*flip) < 0.0f) flip = -flip;
                        values[i + 1] *= flip;
                    }
                }

                /* Normalize the quaternions if not already. Don't attempt to
                   normalize every time to avoid tiny differences, only when
                   the quaternion looks to be off. */
                if(normalizeQuaternions) {
                    for(auto& i: values) if(!i.isNormalized()) {
                        i = i.normalized();
                        hadToRenormalize = true;
                    }
                }

                const auto track = Animation::TrackView<const Float, const Quaternion>{
                    keys, values, interpolation,
                    animationInterpolatorFor<Quaternion>(interpolation),
                    extrapolationBefore, extrapolationAfter};

                tracks[trackId++] = AnimationTrackData{AnimationTrackType::Quaternion,
                    AnimationTrackType::Quaternion, AnimationTrackTargetType::Rotation3D,
                    static_cast<UnsignedInt>(target), track};
            }

            /* Scale */
            if(targetTypes & AnimationTrackTargetType::Scaling3D) {
                const std::size_t keyCount = channel->mNumScalingKeys;
                const auto keys = Containers::arrayCast<Float>(
                    data.sliceSize(dataOffset, keyCount*sizeof(Float)));
                dataOffset += keys.size()*sizeof(keys[0]);
                const auto values = Containers::arrayCast<Vector3>(
                    data.sliceSize(dataOffset, keyCount*sizeof(Vector3)));
                dataOffset += values.size()*sizeof(values[0]);

                for(std::size_t k = 0; k < channel->mNumScalingKeys; ++k) {
                    keys[k] = channel->mScalingKeys[k].mTime / ticksPerSecond;
                    values[k] = Vector3::from(&channel->mScalingKeys[k].mValue[0]);
                }

                const auto track = Animation::TrackView<const Float, const Vector3>{
                    keys, values, interpolation,
                    animationInterpolatorFor<Vector3>(interpolation),
                    extrapolationBefore,
                    extrapolationAfter};
                tracks[trackId++] = AnimationTrackData{AnimationTrackType::Vector3,
                    AnimationTrackType::Vector3, AnimationTrackTargetType::Scaling3D,
                    static_cast<UnsignedInt>(target), track};
            }
        }
    }

    if(hadToRenormalize)
        Warning{} << "Trade::AssimpImporter::animation(): quaternions in some rotation tracks were renormalized";

    return AnimationData{std::move(data), std::move(tracks),
        mergeAnimationClips ? nullptr : _f->scene->mAnimations[id]};
}

UnsignedInt AssimpImporter::doSkin3DCount() const {
    /* If the skins are merged, there's at most one */
    if(_f->mergeSkins)
        return _f->meshesWithBones.isEmpty() ? 0 : 1;

    return _f->meshesWithBones.size();
}

Int AssimpImporter::doSkin3DForName(const Containers::StringView name) {
    /* If the skins are merged, don't report any names */
    if(_f->mergeSkins) return -1;

    if(!_f->skinsForName) {
        _f->skinsForName.emplace();
        _f->skinsForName->reserve(_f->meshesWithBones.size());
        for(std::size_t i = 0; i != _f->meshesWithBones.size(); ++i)
            _f->skinsForName->emplace(
                _f->scene->mMeshes[_f->meshesWithBones[i]]->mName, i);
    }

    const auto found = _f->skinsForName->find(name);
    return found == _f->skinsForName->end() ? -1 : found->second;
}

Containers::String AssimpImporter::doSkin3DName(const UnsignedInt id) {
    /* If the skins are merged, don't report any names */
    if(_f->mergeSkins) return {};
    return _f->scene->mMeshes[_f->meshesWithBones[id]]->mName;
}

Containers::Optional<SkinData3D> AssimpImporter::doSkin3D(const UnsignedInt id) {
    /* Import either a single mesh skin or all of them together. Since Assimp
       gives us no way to enumerate the original skins and assumes that one
       mesh = one skin, we give the users an option to merge them all
       together. */
    const aiMesh* mesh = nullptr;
    Containers::ArrayView<const aiBone* const> bones;
    if(_f->mergeSkins)
        bones = Containers::arrayView(_f->mergedBones);
    else {
        mesh = _f->scene->mMeshes[_f->meshesWithBones[id]];
        bones = Containers::arrayView(mesh->mBones, mesh->mNumBones);
    }

    Containers::Array<UnsignedInt> joints{NoInit, bones.size()};
    /* NoInit for Matrix4 creates an array with a non-default deleter, we're
       not allowed to return those */
    Containers::Array<Matrix4> inverseBindMatrices{ValueInit, bones.size()};
    for(std::size_t i = 0; i != bones.size(); ++i) {
        const aiBone* bone = bones[i];
        const aiNode* node = CORRADE_INTERNAL_ASSERT_EXPRESSION(_f->scene->mRootNode->FindNode(bone->mName));
        joints[i] = _f->nodeIndices.at(node);
        inverseBindMatrices[i] = Matrix4::from(
            reinterpret_cast<const float*>(&bone->mOffsetMatrix)).transposed();
    }

    return SkinData3D{std::move(joints), std::move(inverseBindMatrices), mesh};
}

const void* AssimpImporter::doImporterState() const {
    return _f->scene;
}

}}

CORRADE_PLUGIN_REGISTER(AssimpImporter, Magnum::Trade::AssimpImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
