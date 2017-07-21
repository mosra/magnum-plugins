/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017 Jonathan Hale <squareys@googlemail.com>

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

#include <cstring>
#include <algorithm>
#include <unordered_map>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Utility/Directory.h>

#include <Magnum/Mesh.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/PixelStorage.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include <MagnumPlugins/AnyImageImporter/AnyImageImporter.h>

#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
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
    std::string _filePath;
    Assimp::Importer _importer;
    const aiScene* _scene = nullptr;
    std::vector<aiNode*> _nodes;
    std::vector<std::pair<const aiMaterial*, aiTextureType>> _textures;

    std::unordered_map<const aiNode*, UnsignedInt> _nodeIndices;
    std::unordered_map<const aiNode*, std::pair<Trade::ObjectInstanceType3D, UnsignedInt>> _nodeInstances;
    std::unordered_map<std::string, UnsignedInt> _materialIndicesForName;
    std::unordered_map<const aiMaterial*, UnsignedInt> _textureIndices;
};

AssimpImporter::AssimpImporter() = default;

AssimpImporter::AssimpImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter(manager) {}

AssimpImporter::AssimpImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter(manager, plugin) {}

AssimpImporter::~AssimpImporter() = default;

auto AssimpImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool AssimpImporter::doIsOpened() const { return _f && _f->_scene; }

void AssimpImporter::doOpenData(const Containers::ArrayView<const char> data) {
    if(!_f) {
        _f.reset(new File);
        /* Without aiProcess_JoinIdenticalVertices all meshes are deindexed (wtf?) */
        _f->_scene = _f->_importer.ReadFileFromMemory(data.data(), data.size(), aiProcess_Triangulate|aiProcess_SortByPType|aiProcess_JoinIdenticalVertices);
    }

    if(!_f->_scene) {
        return;
    }

    /* Fill hashmaps for index lookup for materials/textures/meshes/nodes */
    _f->_materialIndicesForName.reserve(_f->_scene->mNumMaterials);

    aiString matName;
    aiString texturePath;
    Int textureIndex = 0;
    for(std::size_t i = 0; i < _f->_scene->mNumMaterials; ++i) {
        const aiMaterial* mat = _f->_scene->mMaterials[i];

        if(mat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
            std::string name = matName.C_Str();
            _f->_materialIndicesForName[name] = i;
        }

        /* Store first possible texture index for this material */
        _f->_textureIndices[mat] = textureIndex;
        for(auto type: {aiTextureType_AMBIENT, aiTextureType_DIFFUSE, aiTextureType_SPECULAR}) {
            if(mat->Get(AI_MATKEY_TEXTURE(type, 0), texturePath) == AI_SUCCESS) {
                std::string path = texturePath.C_Str();
                _f->_textures.emplace_back(mat, type);
                ++textureIndex;
            }
        }
    }

    /* If no nodes, nothing more to do */
    aiNode* root = _f->_scene->mRootNode;
    if(!root) return;

    /* Children + root itself */
    _f->_nodes.reserve(root->mNumChildren + 1);
    _f->_nodes.push_back(root);

    _f->_nodeIndices.reserve(root->mNumChildren + 1);

    /* Insert may invalidate iterators, so we use indices here. */
    for(std::size_t i = 0; i < _f->_nodes.size(); ++i) {
        aiNode* node = _f->_nodes[i];
        _f->_nodeIndices[node] = UnsignedInt(i);

        Containers::ArrayView<aiNode*> children(node->mChildren, node->mNumChildren);
        _f->_nodes.insert(_f->_nodes.end(), children.begin(), children.end());

        if(node->mNumMeshes > 0) {
            /** @todo: Support multiple meshes per node */
            _f->_nodeInstances[node] = {ObjectInstanceType3D::Mesh, node->mMeshes[0]};
        }
    }

    for(std::size_t i = 0; i < _f->_scene->mNumCameras; ++i) {
        const aiNode* cameraNode = _f->_scene->mRootNode->FindNode(_f->_scene->mCameras[i]->mName);
        if(cameraNode) {
            _f->_nodeInstances[cameraNode] = {ObjectInstanceType3D::Camera, i};
        }
    }

    for(std::size_t i = 0; i < _f->_scene->mNumLights; ++i) {
        const aiNode* lightNode = _f->_scene->mRootNode->FindNode(_f->_scene->mLights[i]->mName);
        if(lightNode) {
            _f->_nodeInstances[lightNode] = {ObjectInstanceType3D::Light, i};
        }
    }
}

void AssimpImporter::doOpenFile(const std::string& filename) {
    _f.reset(new File);
    /* Without aiProcess_JoinIdenticalVertices all meshes are deindexed (wtf?) */
    _f->_scene = _f->_importer.ReadFile(filename, aiProcess_Triangulate|aiProcess_SortByPType|aiProcess_JoinIdenticalVertices);
    _f->_filePath = Utility::Directory::path(filename);

    doOpenData({});
}

void AssimpImporter::doClose() {
    _f->_importer.FreeScene();
    _f.reset();
}

Int AssimpImporter::doDefaultScene() { return _f->_scene->mRootNode ? 0 : -1; }

UnsignedInt AssimpImporter::doSceneCount() const { return _f->_scene->mRootNode ? 1 : 0; }

std::optional<SceneData> AssimpImporter::doScene(UnsignedInt) {
    const aiNode* root = _f->_scene->mRootNode;

    std::vector<UnsignedInt> children;
    children.reserve(root->mNumChildren);
    for(std::size_t i = 0; i < root->mNumChildren; ++i)
        children.push_back(_f->_nodeIndices[root->mChildren[i]]);

    return SceneData{{}, std::move(children), root};
}

UnsignedInt AssimpImporter::doCameraCount() const {
    return _f->_scene->mNumCameras;
}

std::optional<CameraData> AssimpImporter::doCamera(UnsignedInt id) {
    const aiCamera* cam = _f->_scene->mCameras[id];
    /** @todo aspect and up vector are not used... */
    return CameraData(Rad(cam->mHorizontalFOV), cam->mClipPlaneNear, cam->mClipPlaneFar, cam);
}

UnsignedInt AssimpImporter::doObject3DCount() const {
    return _f->_nodes.size();
}

Int AssimpImporter::doObject3DForName(const std::string& name) {
    const aiNode* found = _f->_scene->mRootNode->FindNode(aiString(name));
    return found ? _f->_nodeIndices[found] : -1;
}

std::string AssimpImporter::doObject3DName(const UnsignedInt id) {
    return _f->_nodes[id]->mName.C_Str();
}

std::unique_ptr<ObjectData3D> AssimpImporter::doObject3D(const UnsignedInt id) {
    /** @todo support for bone nodes */
    const aiNode* node = _f->_nodes[id];

    /* Gather child indices */
    std::vector<UnsignedInt> children;
    children.reserve(node->mNumChildren);
    for(auto child: Containers::arrayView(node->mChildren, node->mNumChildren))
        children.push_back(_f->_nodeIndices[child]);

    const Matrix4 transformation = Matrix4::from(reinterpret_cast<const float*>(&node->mTransformation));

    auto instance = _f->_nodeInstances.find(node);
    if(instance != _f->_nodeInstances.end()) {
        const ObjectInstanceType3D type = (*instance).second.first;
        const int index = (*instance).second.second;
        if(type == ObjectInstanceType3D::Mesh) {
            const aiMesh* mesh = _f->_scene->mMeshes[index];
            return std::unique_ptr<MeshObjectData3D>(new MeshObjectData3D(children, transformation, index, mesh->mMaterialIndex, node));
        }

        return std::unique_ptr<ObjectData3D>{new ObjectData3D(children, transformation, type, index, node)};
    }

    return std::unique_ptr<ObjectData3D>{new ObjectData3D(children, transformation, node)};
}

UnsignedInt AssimpImporter::doLightCount() const {
    return _f->_scene->mNumLights;
}

std::optional<LightData> AssimpImporter::doLight(UnsignedInt id) {
    const aiLight* l = _f->_scene->mLights[id];

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

UnsignedInt AssimpImporter::doMesh3DCount() const {
    return _f->_scene->mNumMeshes;
}

std::optional<MeshData3D> AssimpImporter::doMesh3D(const UnsignedInt id) {
    const aiMesh* mesh = _f->_scene->mMeshes[id];

    /* Primitive */
    MeshPrimitive primitive;
    if(mesh->mPrimitiveTypes == aiPrimitiveType_POINT) {
        primitive = MeshPrimitive::Points;
    } else if(mesh->mPrimitiveTypes == aiPrimitiveType_LINE) {
        primitive = MeshPrimitive::Lines;
    } else if(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE) {
        primitive = MeshPrimitive::Triangles;
    } else {
        Error() << "Trade::AssimpImporter::mesh3D(): unsupported aiPrimitiveType" << mesh->mPrimitiveTypes;
        return std::nullopt;
    }

    /* Mesh data */
    std::vector<std::vector<Vector3>> positions{{}};
    std::vector<std::vector<Vector2>> textureCoordinates;
    std::vector<std::vector<Vector3>> normals;
    std::vector<std::vector<Color4>> colors;

    /* Import positions */
    auto vertexArray = Containers::arrayCast<Vector3>(Containers::arrayView(mesh->mVertices, mesh->mNumVertices));
    positions.front().assign(vertexArray.begin(), vertexArray.end());

    if(mesh->HasNormals()) {
        normals.emplace_back();
        auto normalArray = Containers::arrayCast<Vector3>(Containers::arrayView(mesh->mNormals, mesh->mNumVertices));
        normals.front().assign(normalArray.begin(), normalArray.end());
    }

    /** @todo only first uv layer (or "channel") supported) */
    textureCoordinates.reserve(mesh->GetNumUVChannels());
    for(std::size_t layer = 0; layer < mesh->GetNumUVChannels(); ++layer) {
        if(mesh->mNumUVComponents[layer] != 2) {
            /** @todo Only 2 dimensional texture coordinates supported in MeshData3D */
            Warning() << "Trade::AssimpImporter::mesh3D(): skipping texture coordinate layer" << layer << "which has" << mesh->mNumUVComponents[layer] << "components per coordinate. Only two dimensional texture coordinates are supported.";
            continue;
        }

        textureCoordinates.emplace_back();
        auto& texCoords = textureCoordinates[layer];
        texCoords.reserve(mesh->mNumVertices);
        for(std::size_t i = 0; i < mesh->mNumVertices; ++i) {
            /* GCC 4.7 has a problem with .x/.y here */
            texCoords.emplace_back(mesh->mTextureCoords[layer][i][0], mesh->mTextureCoords[layer][i][1]);
        }
    }

    colors.reserve(mesh->GetNumColorChannels());
    for(std::size_t layer = 0; layer < mesh->GetNumColorChannels(); ++layer) {
        colors.emplace_back();
        auto colorArray = Containers::arrayCast<Color4>(Containers::arrayView(mesh->mColors[layer], mesh->mNumVertices));
        colors[layer].assign(colorArray.begin(), colorArray.end());
    }

    /* Import indices */
    std::vector<UnsignedInt> indices;
    indices.reserve(mesh->mNumFaces*3);

    for(std::size_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
        const aiFace& face = mesh->mFaces[faceIndex];

        CORRADE_ASSERT(face.mNumIndices <= 3, "Trade::AssimpImporter::mesh3D(): triangulation while loading should have ensured <= 3 vertices per primitive", {});
        for(std::size_t i = 0; i < face.mNumIndices; ++i) {
            indices.push_back(face.mIndices[i]);
        }
    }

    return MeshData3D(primitive, std::move(indices), std::move(positions), std::move(normals), std::move(textureCoordinates), std::move(colors), mesh);
}

UnsignedInt AssimpImporter::doMaterialCount() const { return _f->_scene->mNumMaterials; }

Int AssimpImporter::doMaterialForName(const std::string& name) {
    auto found = _f->_materialIndicesForName.find(name);
    return found != _f->_materialIndicesForName.end() ? found->second : -1;
}

std::string AssimpImporter::doMaterialName(const UnsignedInt id) {
    const aiMaterial* mat = _f->_scene->mMaterials[id];
    aiString name;
    mat->Get(AI_MATKEY_NAME, name);

    return name.C_Str();
}

std::unique_ptr<AbstractMaterialData> AssimpImporter::doMaterial(const UnsignedInt id) {
    /* Put things together */
    const aiMaterial* mat = _f->_scene->mMaterials[id];

    /* Verify that shading mode is either unknown or Phong (not supporting
       anything else ATM) */
    aiShadingMode shadingMode;
    if(mat->Get(AI_MATKEY_SHADING_MODEL, shadingMode) == AI_SUCCESS && shadingMode != aiShadingMode_Phong) {
        Error() << "Trade::AssimpImporter::material(): unsupported shading mode" << shadingMode;
        return {};
    }

    PhongMaterialData::Flags flags;
    Float shininess;
    aiString texturePath;
    aiColor3D color;

    if(mat->Get(AI_MATKEY_TEXTURE(aiTextureType_AMBIENT, 0), texturePath) == AI_SUCCESS)
        flags |= PhongMaterialData::Flag::AmbientTexture;
    if(mat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), texturePath) == AI_SUCCESS)
        flags |= PhongMaterialData::Flag::DiffuseTexture;
    if(mat->Get(AI_MATKEY_TEXTURE(aiTextureType_SPECULAR, 0), texturePath) == AI_SUCCESS)
        flags |= PhongMaterialData::Flag::SpecularTexture;
    /** @todo many more types supported in assimp */

    /* Key always present, default 0.0f */
    mat->Get(AI_MATKEY_SHININESS, shininess);

    std::unique_ptr<PhongMaterialData> data{new PhongMaterialData(flags, shininess, mat)};

    /* Key always present, default black */
    mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
    if(!(flags & PhongMaterialData::Flag::AmbientTexture))
        data->ambientColor() = Color3(color);

    /* Key always present, default black */
    mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    if(!(flags & PhongMaterialData::Flag::DiffuseTexture))
        data->diffuseColor() = Color3(color);

    /* Key always present, default black */
    mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
    if(!(flags & PhongMaterialData::Flag::SpecularTexture))
        data->specularColor() = Color3(color);

    return std::move(data);
}

UnsignedInt AssimpImporter::doTextureCount() const { return _f->_textures.size(); }

std::optional<TextureData> AssimpImporter::doTexture(const UnsignedInt id) {
    auto toWrapping = [](aiTextureMapMode mapMode) {
        switch (mapMode) {
            case aiTextureMapMode_Wrap:
                return Sampler::Wrapping::Repeat;
            case aiTextureMapMode_Decal:
                Warning() << "Trade::AssimpImporter::texture(): no wrapping "
                    "enum to match aiTextureMapMode_Decal, using "
                    "Sampler::Wrapping::ClampToEdge";
                return Sampler::Wrapping::ClampToEdge;
            case aiTextureMapMode_Clamp:
                return Sampler::Wrapping::ClampToEdge;
            case aiTextureMapMode_Mirror:
                return Sampler::Wrapping::MirroredRepeat;
            default:
                Warning() << "Trade::AssimpImporter::texture(): unknown "
                    "aiTextureMapMode" << mapMode << Debug::nospace << ", "
                    "using Sampler::Wrapping::ClampToEdge";
                return Sampler::Wrapping::ClampToEdge;
        }
    };

    aiTextureMapMode mapMode;
    const aiMaterial* mat = _f->_textures[id].first;
    const aiTextureType type = _f->_textures[id].second;
    Sampler::Wrapping wrappingU = Sampler::Wrapping::ClampToEdge;
    Sampler::Wrapping wrappingV = Sampler::Wrapping::ClampToEdge;
    if(mat->Get(AI_MATKEY_MAPPINGMODE_U(type, 0), mapMode) == AI_SUCCESS)
        wrappingU = toWrapping(mapMode);
    if(mat->Get(AI_MATKEY_MAPPINGMODE_V(type, 0), mapMode) == AI_SUCCESS)
        wrappingV = toWrapping(mapMode);

    return TextureData{TextureData::Type::Texture2D,
        Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Mipmap::Linear,
        {wrappingU, wrappingV, Sampler::Wrapping::ClampToEdge}, id, &_f->_textures[id]};
}

UnsignedInt AssimpImporter::doImage2DCount() const { return _f->_textures.size(); }

std::optional<ImageData2D> AssimpImporter::doImage2D(const UnsignedInt id) {
    CORRADE_ASSERT(manager(), "Trade::AssimpImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    const aiMaterial* mat;
    aiTextureType type;
    std::tie(mat, type) = _f->_textures[id];

    aiString texturePath;
    if(mat->Get(AI_MATKEY_TEXTURE(type, 0), texturePath) != AI_SUCCESS) {
        Error() << "Trade::AssimpImporter::image2D(): error getting path for texture" << id;
        return std::nullopt;
    }

    std::string path = texturePath.C_Str();
    /* If path is prefixed with '*', load embedded texture */
    if(path[0] == '*') {
        char* err;
        const std::string indexStr = path.substr(1);
        const char* str = indexStr.c_str();

        const Int index = Int(std::strtol(str, &err, 10));
        if(err == nullptr || err == str) {
            Error() << "Trade::AssimpImporter::image2D(): embedded texture path did not contain a valid integer string";
            return std::nullopt;
        }

        const aiTexture* texture = _f->_scene->mTextures[index];
        if(texture->mHeight == 0) {
            /* Compressed image data */
            auto textureData = Containers::ArrayView<const char>(reinterpret_cast<const char*>(texture->pcData), texture->mWidth);

            std::string importerName;
            if(texture->CheckFormat("dds")) {
                importerName = "DdsImporter";
            } else if(texture->CheckFormat("jpg")) {
                importerName = "JpegImporter";
            } else if(texture->CheckFormat("pcx")) {
                importerName = "PxcImporter";
            } else if(texture->CheckFormat("png") || std::strncmp(textureData.suffix(1).data(), "PNG", 3) == 0) {
                importerName = "PngImporter";
            } else {
                Error() << "Trade::AssimpImporter::image2D(): could not detect filetype of embedded data";
                return std::nullopt;
            }

            std::unique_ptr<Trade::AbstractImporter> importer = manager()->loadAndInstantiate(importerName);
            if(!importer) {
                Error() << "Trade::AssimpImporter::image2D(): could not find importer for embedded data";
                return std::nullopt;
            }

            importer->openData(textureData);
            return importer->image2D(0);

        /* Uncompressed image data */
        } else {
            Error() << "Trade::AssimpImporter::image2D(): uncompressed embedded image data is not supported";
            return std::nullopt;
        }

    /* Load external texture */
    } else {
        AnyImageImporter importer{*manager()};
        importer.openFile(Utility::Directory::join(_f->_filePath, path));
        return importer.image2D(0);
    }
}

const void* AssimpImporter::doImporterState() const {
    return _f->_scene;
}

}}
