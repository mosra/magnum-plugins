/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2018 Konstantinos Chatzilygeroudis <costashatz@gmail.com>
    Copyright © 2019, 2020 Max Schwarz <max.schwarz@ais.uni-bonn.de>

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

#include <unordered_map>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/String.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/PixelStorage.h>
#include <Magnum/Trade/ArrayAllocator.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <MagnumPlugins/AnyImageImporter/AnyImageImporter.h>

#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/scene.h>

namespace Magnum { namespace Math { namespace Implementation {

template<> struct VectorConverter<3, Float, aiColor3D> {
    static Vector<3, Float> from(const aiColor3D& other) {
        return {other.r, other.g, other.b};
    }
};

}}}

namespace Magnum { namespace Trade {

struct AssimpImporter::File {
    Containers::Optional<std::string> filePath;
    const aiScene* scene = nullptr;
    std::vector<aiNode*> nodes;
    std::vector<std::tuple<const aiMaterial*, aiTextureType, UnsignedInt>> textures;
    std::vector<std::pair<const aiMaterial*, aiTextureType>> images;

    std::unordered_map<const aiNode*, UnsignedInt> nodeIndices;
    std::unordered_map<const aiNode*, std::pair<Trade::ObjectInstanceType3D, UnsignedInt>> nodeInstances;
    std::unordered_map<std::string, UnsignedInt> materialIndicesForName;
    std::unordered_map<const aiMaterial*, UnsignedInt> textureIndices;

    /* Mapping for multi-mesh nodes:
       (in the following, "node" is an aiNode,
        "object" is Magnum::Trade::ObjectData3D)

        -   objectMap.size() is the count of objects reported to the user
        -   nodeMap.size() is the count of original nodes in the file + 1
        -   objectMap[id] is a pair of (original node ID, mesh ID)
        -   nodeMap[j] points to the first item in objectMap for
            node ID `j` -- which also translates the original ID to reported ID
        -   nodeMap[j + 1] - nodeMap[j] is count of objects for
            original object ID `j` (or number of primitives in given object)

      Hierarchy-wise, the subsequent nodes are direct children of
      the first, have no transformation or other children and point to the
      subsequent meshes.
    */
    std::vector<std::pair<std::size_t, std::size_t>> objectMap;
    std::vector<std::size_t> nodeMap;

    UnsignedInt imageImporterId = ~UnsignedInt{};
    Containers::Optional<AnyImageImporter> imageImporter;
};

namespace {

void fillDefaultConfiguration(Utility::ConfigurationGroup& conf) {
    /** @todo horrible workaround, fix this properly */
    Utility::ConfigurationGroup& postprocess = *conf.addGroup("postprocess");
    postprocess.setValue("JoinIdenticalVertices", true);
    postprocess.setValue("Triangulate", true);
    postprocess.setValue("SortByPType", true);
}

}

AssimpImporter::AssimpImporter(): _importer{new Assimp::Importer} {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

AssimpImporter::AssimpImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter(manager), _importer{new Assimp::Importer} {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

AssimpImporter::AssimpImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter(manager, plugin), _importer{new Assimp::Importer}  {}

AssimpImporter::~AssimpImporter() = default;

ImporterFeatures AssimpImporter::doFeatures() const { return ImporterFeature::OpenData|ImporterFeature::OpenState|ImporterFeature::FileCallback; }

bool AssimpImporter::doIsOpened() const { return _f && _f->scene; }

namespace {

struct IoStream: Assimp::IOStream {
    explicit IoStream(std::string filename, Containers::ArrayView<const char> data): filename{std::move(filename)}, _data{data}, _pos{} {}

    /* mimics fread() / fseek() */
    std::size_t Read(void* buffer, std::size_t size, std::size_t count) override {
        const Containers::ArrayView<const char> slice = _data.suffix(_pos);
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
        CORRADE_ASSERT_UNREACHABLE();
    }
    void Flush() override {
        CORRADE_ASSERT_UNREACHABLE();
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
        CORRADE_INTERNAL_ASSERT(mode == std::string{"rb"});
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

void AssimpImporter::doSetFileCallback(Containers::Optional<Containers::ArrayView<const char>>(*callback)(const std::string&, InputFileCallbackPolicy, void*), void* userData) {
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
    const Utility::ConfigurationGroup& postprocess = *conf.group("postprocess");
    #define _c(val) if(postprocess.value<bool>(#val)) flags |= aiProcess_ ## val;
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
    #undef _c
    return flags;
}

}

void AssimpImporter::doOpenData(const Containers::ArrayView<const char> data) {
    if(!_f) {
        _f.reset(new File);
        /* File callbacks are set up in doSetFileCallbacks() */
        if(!(_f->scene = _importer->ReadFileFromMemory(data.data(), data.size(), flagsFromConfiguration(configuration())))) {
            Error{} << "Trade::AssimpImporter::openData(): loading failed:" << _importer->GetErrorString();
            return;
        }
    }

    CORRADE_INTERNAL_ASSERT(_f->scene);

    /* Fill hashmaps for index lookup for materials/textures/meshes/nodes */
    _f->materialIndicesForName.reserve(_f->scene->mNumMaterials);

    aiString matName;
    aiString texturePath;
    Int textureIndex = 0;
    std::unordered_map<std::string, UnsignedInt> uniqueImages;
    for(std::size_t i = 0; i < _f->scene->mNumMaterials; ++i) {
        const aiMaterial* mat = _f->scene->mMaterials[i];

        if(mat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
            std::string name = matName.C_Str();
            _f->materialIndicesForName[name] = i;
        }

        /* Store first possible texture index for this material, next textures
           use successive indices. For images ensure we have an unique set so
           each file isn't imported more than once. */
        _f->textureIndices[mat] = textureIndex;
        for(auto type: {aiTextureType_AMBIENT, aiTextureType_DIFFUSE, aiTextureType_SPECULAR}) {
            if(mat->Get(AI_MATKEY_TEXTURE(type, 0), texturePath) == AI_SUCCESS) {
                auto uniqueImage = uniqueImages.emplace(texturePath.C_Str(), _f->images.size());
                if(uniqueImage.second) _f->images.emplace_back(mat, type);

                _f->textures.emplace_back(mat, type, uniqueImage.first->second);
                ++textureIndex;
            }
        }
    }

    /* For some formats (such as COLLADA) Assimp fails to open the scene if
       there are no nodes, so there this is always non-null. For other formats
       (such as glTF) Assimp happily provides a null root node, even thought
       that's not the documented behavior. */
    aiNode* const root = _f->scene->mRootNode;
    if(root) {
        /* I would assert here on !root->mNumMeshes to verify I didn't miss
           anything in the root node, but at least for COLLADA, if the file has
           no meshes, it adds some bogus one, thinking it's a skeleton-only
           file and trying to be helpful. Ugh.
           https://github.com/assimp/assimp/blob/92078bc47c462d5b643aab3742a8864802263700/code/ColladaLoader.cpp#L225 */

        /* If there is more than just a root node, extract children of the root
           node, as we treat the root node as the scene here and it has no
           transformation or anything attached. */
        if(root->mNumChildren) {
            _f->nodes.reserve(root->mNumChildren);
            _f->nodes.insert(_f->nodes.end(), root->mChildren, root->mChildren + root->mNumChildren);
            _f->nodeIndices.reserve(root->mNumChildren);

        /* In some pathological cases there's just one root node --- for
           example the DART integration depends on that. Import it as a single
           node. */
        } else {
            _f->nodes.push_back(root);
            _f->nodeIndices.reserve(1);
        }

        /* Insert may invalidate iterators, so we use indices here. */
        /* Treat nodes with multiple meshes as separate objects. */
        _f->nodeMap.emplace_back(0);
        for(std::size_t i = 0; i < _f->nodes.size(); ++i) {
            aiNode* node = _f->nodes[i];
            _f->nodeIndices[node] = UnsignedInt(i);

            _f->nodes.insert(_f->nodes.end(), node->mChildren, node->mChildren + node->mNumChildren);

            _f->objectMap.emplace_back(i, 0);
            if(node->mNumMeshes > 0) {
                /* Attach the first mesh directly to the node */
                _f->nodeInstances[node] = {ObjectInstanceType3D::Mesh, node->mMeshes[0]};

                for(std::size_t j = 1; j < node->mNumMeshes; ++j) {
                    _f->objectMap.emplace_back(i, j);
                }
            }

            _f->nodeMap.emplace_back(_f->objectMap.size());
        }

        for(std::size_t i = 0; i < _f->scene->mNumCameras; ++i) {
            const aiNode* cameraNode = _f->scene->mRootNode->FindNode(_f->scene->mCameras[i]->mName);
            if(cameraNode) {
                _f->nodeInstances[cameraNode] = {ObjectInstanceType3D::Camera, i};
            }
        }

        for(std::size_t i = 0; i < _f->scene->mNumLights; ++i) {
            const aiNode* lightNode = _f->scene->mRootNode->FindNode(_f->scene->mLights[i]->mName);
            if(lightNode) {
                _f->nodeInstances[lightNode] = {ObjectInstanceType3D::Light, i};
            }
        }
    }
}

void AssimpImporter::doOpenState(const void* state, const std::string& filePath) {
    _f.reset(new File);
    _f->scene = static_cast<const aiScene*>(state);
    _f->filePath = filePath;

    doOpenData({});
}

void AssimpImporter::doOpenFile(const std::string& filename) {
    _f.reset(new File);
    _f->filePath = Utility::Directory::path(filename);

    /* File callbacks are set up in doSetFileCallbacks() */
    if(!(_f->scene = _importer->ReadFile(filename, flagsFromConfiguration(configuration())))) {
        Error{} << "Trade::AssimpImporter::openFile(): failed to open" << filename << Debug::nospace << ":" << _importer->GetErrorString();
        return;
    }

    doOpenData({});
}

void AssimpImporter::doClose() {
    _importer->FreeScene();
    _f.reset();
}

Int AssimpImporter::doDefaultScene() { return _f->scene->mRootNode ? 0 : -1; }

UnsignedInt AssimpImporter::doSceneCount() const { return _f->scene->mRootNode ? 1 : 0; }

Containers::Optional<SceneData> AssimpImporter::doScene(UnsignedInt) {
    const aiNode* root = _f->scene->mRootNode;

    std::vector<UnsignedInt> children;
    /* In consistency with the distinction in doOpenData(), if the root node
       has children, add them directly (and treat the root node as the scene) */
    if(root->mNumChildren) {
        children.reserve(root->mNumChildren);
        for(std::size_t i = 0; i < root->mNumChildren; ++i)
            children.push_back(_f->nodeMap[_f->nodeIndices[root->mChildren[i]]]);

    /* Otherwise there's just the root node, which is at index 0 */
    } else children.push_back(0);

    return SceneData{{}, std::move(children), root};
}

UnsignedInt AssimpImporter::doCameraCount() const {
    return _f->scene->mNumCameras;
}

Containers::Optional<CameraData> AssimpImporter::doCamera(UnsignedInt id) {
    const aiCamera* cam = _f->scene->mCameras[id];
    /** @todo aspect and up vector are not used... */
    return CameraData{CameraType::Perspective3D, Rad(cam->mHorizontalFOV), 1.0f, cam->mClipPlaneNear, cam->mClipPlaneFar, cam};
}

UnsignedInt AssimpImporter::doObject3DCount() const {
    return _f->objectMap.size();
}

Int AssimpImporter::doObject3DForName(const std::string& name) {
    const aiNode* found = _f->scene->mRootNode->FindNode(aiString(name));
    return found ? _f->nodeMap[_f->nodeIndices[found]] : -1;
}

std::string AssimpImporter::doObject3DName(const UnsignedInt id) {
    return _f->nodes[_f->objectMap[id].first]->mName.C_Str();
}

Containers::Pointer<ObjectData3D> AssimpImporter::doObject3D(const UnsignedInt id) {
    const auto& spec = _f->objectMap[id];
    const UnsignedInt nodeId = spec.first;
    const aiNode* node = _f->nodes[spec.first];


    /* Is this the first mesh of the aiNode? */
    if(spec.second == 0) {
        /** @todo support for bone nodes */

        /* Object children: first add extra objects caused by multi-mesh nodes,
           after that the usual children. */
        std::vector<UnsignedInt> children;
        const std::size_t extraChildrenCount = _f->nodeMap[nodeId + 1] - _f->nodeMap[nodeId] - 1;
        children.reserve(extraChildrenCount + node->mNumChildren);

        for(std::size_t i = 0; i != extraChildrenCount; ++i)
            children.push_back(_f->nodeMap[nodeId] + i + 1);

        for(auto child: Containers::arrayView(node->mChildren, node->mNumChildren))
            children.push_back(_f->nodeMap[_f->nodeIndices[child]]);

        /* aiMatrix4x4 is always row-major, transpose */
        const Matrix4 transformation = Matrix4::from(reinterpret_cast<const float*>(&node->mTransformation)).transposed();

        auto instance = _f->nodeInstances.find(node);
        if(instance != _f->nodeInstances.end()) {
            const ObjectInstanceType3D type = (*instance).second.first;
            const int index = (*instance).second.second;
            if(type == ObjectInstanceType3D::Mesh) {
                const aiMesh* mesh = _f->scene->mMeshes[index];
                return Containers::pointer(new MeshObjectData3D(children, transformation, index, mesh->mMaterialIndex, node));
            }

            return Containers::pointer(new ObjectData3D(children, transformation, type, index, node));
        }

        return Containers::pointer(new ObjectData3D(children, transformation, node));
    } else {
        /* Additional mesh for the referenced node. This is represented as a
           child of the referenced node with identity transformation */

        const aiMesh* mesh = _f->scene->mMeshes[node->mMeshes[spec.second]];

        Vector3 translation{};
        Quaternion rotation{};
        Vector3 scaling{1.0};
        return Containers::pointer(new MeshObjectData3D(
            {},
            translation, rotation, scaling,
            node->mMeshes[spec.second], mesh->mMaterialIndex, node)
        );
    }
}

UnsignedInt AssimpImporter::doLightCount() const {
    return _f->scene->mNumLights;
}

Containers::Optional<LightData> AssimpImporter::doLight(UnsignedInt id) {
    const aiLight* l = _f->scene->mLights[id];

    LightData::Type lightType;
    if(l->mType == aiLightSource_DIRECTIONAL) {
        lightType = LightData::Type::Infinite;
    } else if(l->mType == aiLightSource_POINT) {
        lightType = LightData::Type::Point;
    } else if(l->mType == aiLightSource_SPOT) {
        lightType = LightData::Type::Spot;
    } else {
        Error() << "Trade::AssimpImporter::light(): light type" << l->mType << "is not supported";
        return {};
    }

    Color3 ambientColor = Color3(l->mColorDiffuse);

    /** @todo angle inner/outer cone, linear/quadratic/constant attenuation, ambient/specular color are not used */
    return LightData(lightType, ambientColor, 1.0f, l);
}

UnsignedInt AssimpImporter::doMeshCount() const {
    return _f->scene->mNumMeshes;
}

Containers::Optional<MeshData> AssimpImporter::doMesh(const UnsignedInt id, UnsignedInt) {
    const aiMesh* mesh = _f->scene->mMeshes[id];

    /* Primitive */
    MeshPrimitive primitive;
    if(mesh->mPrimitiveTypes == aiPrimitiveType_POINT) {
        primitive = MeshPrimitive::Points;
    } else if(mesh->mPrimitiveTypes == aiPrimitiveType_LINE) {
        primitive = MeshPrimitive::Lines;
    } else if(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE) {
        primitive = MeshPrimitive::Triangles;
    } else {
        Error() << "Trade::AssimpImporter::mesh(): unsupported aiPrimitiveType" << mesh->mPrimitiveTypes;
        return Containers::NullOpt;
    }

    /* Gather all attributes. Position is there always, others are optional */
    std::size_t attributeCount = 1;
    std::ptrdiff_t stride = sizeof(Vector3);
    if(mesh->HasNormals()) {
        ++attributeCount;
        stride += sizeof(Vector3);
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

    /* Allocate vertex data, fill in the attributes */
    const UnsignedInt vertexCount = mesh->mNumVertices;
    Containers::Array<char> vertexData{Containers::NoInit, std::size_t(stride)*vertexCount};
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

    /* Check we pre-calculated well */
    CORRADE_INTERNAL_ASSERT(attributeOffset == std::size_t(stride));
    CORRADE_INTERNAL_ASSERT(attributeIndex == attributeCount);

    /* Import indices. There doesn't seem to be any shortcut to just copy all
       index data in a single go, so having to iterate over faces. Ugh. */
    Containers::Array<UnsignedInt> indexData;
    Containers::arrayReserve<ArrayAllocator>(indexData, mesh->mNumFaces*3);
    for(std::size_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
        const aiFace& face = mesh->mFaces[faceIndex];
        CORRADE_ASSERT(face.mNumIndices <= 3, "Trade::AssimpImporter::mesh(): triangulation while loading should have ensured <= 3 vertices per primitive", {});

        Containers::arrayAppend<ArrayAllocator>(indexData, {face.mIndices, face.mNumIndices});
    }

    MeshIndexData indices{indexData};
    return MeshData{primitive,
        Containers::arrayAllocatorCast<char, ArrayAllocator>(std::move(indexData)),
        indices,
        std::move(vertexData), std::move(attributeData),
        MeshData::ImplicitVertexCount, mesh};
}

UnsignedInt AssimpImporter::doMaterialCount() const { return _f->scene->mNumMaterials; }

Int AssimpImporter::doMaterialForName(const std::string& name) {
    auto found = _f->materialIndicesForName.find(name);
    return found != _f->materialIndicesForName.end() ? found->second : -1;
}

std::string AssimpImporter::doMaterialName(const UnsignedInt id) {
    const aiMaterial* mat = _f->scene->mMaterials[id];
    aiString name;
    mat->Get(AI_MATKEY_NAME, name);

    return name.C_Str();
}

Containers::Pointer<AbstractMaterialData> AssimpImporter::doMaterial(const UnsignedInt id) {
    /* Put things together */
    const aiMaterial* mat = _f->scene->mMaterials[id];

    /* Verify that shading mode is either unknown or Phong (not supporting
       anything else ATM) */
    aiShadingMode shadingMode;
    if(mat->Get(AI_MATKEY_SHADING_MODEL, shadingMode) == AI_SUCCESS && shadingMode != aiShadingMode_Phong) {
        Error() << "Trade::AssimpImporter::material(): unsupported shading mode" << shadingMode;
        return {};
    }

    PhongMaterialData::Flags flags;
    aiString texturePath;
    aiColor3D color;

    if(mat->Get(AI_MATKEY_TEXTURE(aiTextureType_AMBIENT, 0), texturePath) == AI_SUCCESS)
        flags |= PhongMaterialData::Flag::AmbientTexture;
    if(mat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), texturePath) == AI_SUCCESS)
        flags |= PhongMaterialData::Flag::DiffuseTexture;
    if(mat->Get(AI_MATKEY_TEXTURE(aiTextureType_SPECULAR, 0), texturePath) == AI_SUCCESS)
        flags |= PhongMaterialData::Flag::SpecularTexture;
    /** @todo many more types supported in assimp */

    /* Shininess is *not* always present (for example in STL models), default
       to 0 */
    Float shininess = 0.0f;
    mat->Get(AI_MATKEY_SHININESS, shininess);

    Containers::Pointer<PhongMaterialData> data{Containers::InPlaceInit, flags, MaterialAlphaMode::Opaque, 0.5f, shininess, mat};

    /* Key always present, default black */
    mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
    UnsignedInt firstTextureIndex = _f->textureIndices[mat];
    if(flags & PhongMaterialData::Flag::AmbientTexture) {
        data->ambientTexture() = firstTextureIndex++;
    } else {
        data->ambientColor() = Color3(color);

        /* Assimp 4.1 forces ambient color to white for STL models. That's just
           plain wrong, so we force it back to black (and emit a warning, so in
           the very rare case when someone would actually want white ambient,
           they'll know it got overriden). Fixed by
           https://github.com/assimp/assimp/pull/2563, not released yet. */
        if(data->ambientColor() == Color4{1.0f}) {
            Warning{} << "Trade::AssimpImporter::material(): white ambient detected, forcing back to black";
            data->ambientColor() = Color3{0.0f};
        }
    }

    /* Key always present, default black */
    mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    if(flags & PhongMaterialData::Flag::DiffuseTexture)
        data->diffuseTexture() = firstTextureIndex++;
    else
        data->diffuseColor() = Color3(color);

    /* Key always present, default black */
    mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
    if(flags & PhongMaterialData::Flag::SpecularTexture)
        data->specularTexture() = firstTextureIndex++;
    else
        data->specularColor() = Color3(color);

    /* Needs to be explicit on GCC 4.8 and Clang 3.8 so it can properly upcast
       the pointer. Just std::move() works as well, but that gives a warning
       on GCC 9. */
    return Containers::Pointer<AbstractMaterialData>{std::move(data)};
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
    const aiMaterial* mat = std::get<0>(_f->textures[id]);
    const aiTextureType type = std::get<1>(_f->textures[id]);
    SamplerWrapping wrappingU = SamplerWrapping::ClampToEdge;
    SamplerWrapping wrappingV = SamplerWrapping::ClampToEdge;
    if(mat->Get(AI_MATKEY_MAPPINGMODE_U(type, 0), mapMode) == AI_SUCCESS)
        wrappingU = toWrapping(mapMode);
    if(mat->Get(AI_MATKEY_MAPPINGMODE_V(type, 0), mapMode) == AI_SUCCESS)
        wrappingV = toWrapping(mapMode);

    return TextureData{TextureData::Type::Texture2D,
        SamplerFilter::Linear, SamplerFilter::Linear, SamplerMipmap::Linear,
        {wrappingU, wrappingV, SamplerWrapping::ClampToEdge}, std::get<2>(_f->textures[id]), &_f->textures[id]};
}

UnsignedInt AssimpImporter::doImage2DCount() const { return _f->images.size(); }

AbstractImporter* AssimpImporter::setupOrReuseImporterForImage(const UnsignedInt id, const char* const errorPrefix) {
    const aiMaterial* mat;
    aiTextureType type;
    std::tie(mat, type) = _f->images[id];

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

    std::string path = texturePath.C_Str();
    /* If path is prefixed with '*', load embedded texture */
    if(path[0] == '*') {
        char* err;
        const std::string indexStr = path.substr(1);
        const char* str = indexStr.c_str();

        const Int index = Int(std::strtol(str, &err, 10));
        if(err == nullptr || err == str) {
            Error{} << errorPrefix << "embedded texture path did not contain a valid integer string";
            return nullptr;
        }

        const aiTexture* texture = _f->scene->mTextures[index];
        if(texture->mHeight == 0) {
            /* Compressed image data */
            auto textureData = Containers::ArrayView<const char>(reinterpret_cast<const char*>(texture->pcData), texture->mWidth);

            AnyImageImporter importer{*manager()};
            if(!importer.openData(textureData))
                return nullptr;
            return &_f->imageImporter.emplace(std::move(importer));

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

        AnyImageImporter importer{*manager()};
        if(fileCallback()) importer.setFileCallback(fileCallback(), fileCallbackUserData());
        /* Assimp doesn't trim spaces from the end of image paths in OBJ
           materials so we have to. See the image-filename-space.mtl test. */
        if(!importer.openFile(Utility::String::trim(Utility::Directory::join(_f->filePath ? *_f->filePath : "", path))))
            return nullptr;
        return &_f->imageImporter.emplace(std::move(importer));
    }
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

const void* AssimpImporter::doImporterState() const {
    return _f->scene;
}

}}

CORRADE_PLUGIN_REGISTER(AssimpImporter, Magnum::Trade::AssimpImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.1")
