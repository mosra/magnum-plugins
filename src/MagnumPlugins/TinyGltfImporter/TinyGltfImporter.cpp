/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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

#include "TinyGltfImporter.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/String.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Math/Math.h>
#include <Magnum/Mesh.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>

#include "MagnumPlugins/StbImageImporter/StbImageImporter.h"
#include "MagnumPlugins/AnyImageImporter/AnyImageImporter.h"

#define TINYGLTF_IMPLEMENTATION
/* Opt out of tinygltf stb_image dependency */
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
/* Opt out of loading external images */
#define TINYGLTF_NO_EXTERNAL_IMAGE

/* Tinygltf includes some windows headers, avoid including more than ncessary
   to speed up compilation. */
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#include "tiny_gltf.h"
#undef near
#undef far

namespace Magnum { namespace Trade {

using namespace Magnum::Math::Literals;

namespace {

bool loadImageData(tinygltf::Image*, std::string*, int, int, const unsigned char*, int, void*) {
    /* Bypass tinygltf image loading and load the image on demand in image2D instead. */
    return true;
}

}

struct TinyGltfImporter::Document {
    std::string filePath;

    tinygltf::Model model;

    std::unordered_map<std::string, Int> nodesForName,
        materialsForName;

    bool open = false;
};

TinyGltfImporter::TinyGltfImporter() = default;

TinyGltfImporter::TinyGltfImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

TinyGltfImporter::~TinyGltfImporter() = default;

auto TinyGltfImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool TinyGltfImporter::doIsOpened() const { return !!_d && _d->open; }

void TinyGltfImporter::doClose() { _d = nullptr; }

void TinyGltfImporter::doOpenFile(const std::string& filename) {
    _d.reset(new Document);
    _d->filePath = Utility::Directory::path(filename);
    AbstractImporter::doOpenFile(filename);
}

void TinyGltfImporter::doOpenData(const Containers::ArrayView<const char> data) {
    tinygltf::TinyGLTF loader;
    std::string err;

    if(!_d) _d.reset(new Document);

    loader.SetImageLoader(&loadImageData, nullptr);

    _d->open = true;
    if(data.size() >= 4 && strncmp(data.data(), "glTF", 4) == 0) {
        std::vector<UnsignedByte> chars(data.begin(), data.end());
        _d->open = loader.LoadBinaryFromMemory(&_d->model, &err, chars.data(), data.size(), _d->filePath, tinygltf::SectionCheck::NO_REQUIRE);
    } else {
        _d->open = loader.LoadASCIIFromString(&_d->model, &err, data.data(), data.size(), _d->filePath, tinygltf::SectionCheck::NO_REQUIRE);
    }

    if(!_d->open) {
        Error() << "Trade::TinyGltfImporter::openFile(): error opening file:" << err;
        doClose();
        return;
    }

    for(std::size_t i = 0; i < _d->model.nodes.size(); i++)
        _d->nodesForName.emplace(_d->model.nodes[i].name, i);

    for(std::size_t i = 0; i < _d->model.materials.size(); i++)
        _d->materialsForName.emplace(_d->model.materials[i].name, i);
}

Int TinyGltfImporter::doDefaultScene() {
    return _d->model.defaultScene;
}

Containers::Optional<CameraData> TinyGltfImporter::doCamera(UnsignedInt id) {
    const tinygltf::Camera& camera = _d->model.cameras[id];

    Float far, near;
    Rad fov;

    if(camera.type == "perspective") {
        far = camera.perspective.zfar;
        near = camera.perspective.znear;
        fov = Rad{camera.perspective.yfov};
    } else if(camera.type == "orthographic") {
        far = camera.orthographic.zfar;
        near = camera.orthographic.znear;
    } else CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    return CameraData{fov, near, far, &camera};
}

UnsignedInt TinyGltfImporter::doCameraCount() const {
    return _d->model.cameras.size();
}

Containers::Optional<LightData> TinyGltfImporter::doLight(UnsignedInt id) {
    const tinygltf::Light& light = _d->model.lights[id];

    Color3 lightColor{float(light.color[0]), float(light.color[1]), float(light.color[2])};
    Float lightIntensity{1.0f}; /* not supported by tinygltf */

    LightData::Type lightType;

    if(light.type == "point") {
        lightType = LightData::Type::Point;
    } else if(light.type == "spot") {
        lightType = LightData::Type::Spot;
    } else if(light.type == "directional") {
        lightType = LightData::Type::Infinite;
    } else if(light.type == "ambient") {
        Error() << "Trade::TinyGltfImporter::light(): unsupported value for light type:" << light.type;
        return Containers::NullOpt;
    /* LCOV_EXCL_START */
    } else {
        Error() << "Trade::TinyGltfImporter::light(): invalid value for light type:" << light.type;
        return Containers::NullOpt;
    }
    /* LCOV_EXCL_STOP */

    return LightData{lightType, lightColor, lightIntensity, &light};
}

UnsignedInt TinyGltfImporter::doLightCount() const {
    return _d->model.lights.size();
}

UnsignedInt TinyGltfImporter::doSceneCount() const { return _d->model.scenes.size(); }

Containers::Optional<SceneData> TinyGltfImporter::doScene(UnsignedInt id) {
    std::vector<UnsignedInt> children;
    const tinygltf::Scene& scene = _d->model.scenes[id];
    for(const Int node: scene.nodes) children.push_back(node);

    return SceneData{{}, children};
}

UnsignedInt TinyGltfImporter::doObject3DCount() const {
    return _d->model.nodes.size();
}

std::string TinyGltfImporter::doObject3DName(UnsignedInt id) {
    return _d->model.nodes[id].name;
}

Int TinyGltfImporter::doObject3DForName(const std::string& name) {
    const auto found = _d->nodesForName.find(name);
    return found == _d->nodesForName.end() ? -1 : found->second;
}

std::unique_ptr<ObjectData3D> TinyGltfImporter::doObject3D(UnsignedInt id) {
    const tinygltf::Node& node = _d->model.nodes[id];

    CORRADE_INTERNAL_ASSERT(node.rotation.size() == 0 || node.rotation.size() == 4);
    CORRADE_INTERNAL_ASSERT(node.translation.size() == 0 || node.translation.size() == 3);
    CORRADE_INTERNAL_ASSERT(node.scale.size() == 0 || node.scale.size() == 3);
    /* Ensure we have either a matrix or T-R-S */
    CORRADE_INTERNAL_ASSERT(node.matrix.size() == 0 ||
        (node.matrix.size() == 16 && node.translation.size() == 0 && node.rotation.size() == 0 && node.scale.size() == 0));

    std::vector<UnsignedInt> children(node.children.begin(), node.children.end());

    Matrix4 transformation;
    if(node.rotation.size() == 4) {
        const Vector3 vector(Vector3d::from(node.rotation.data()));
        const auto scalar = node.rotation[3];
        transformation = Matrix4::from(Quaternion(vector, scalar).normalized().toMatrix(), {});
    }
    if(node.translation.size() == 3) {
        const Vector3 vector(Vector3d::from(node.translation.data()));
        Matrix4 translation = Matrix4::translation(vector);
        transformation = transformation*translation;
    }
    if(node.scale.size() == 3) {
        const Vector3 vector(Vector3d::from(node.scale.data()));
        Matrix4 scale = Matrix4::scaling(vector);
        transformation = transformation*scale;
    }
    if(node.matrix.size() == 16) {
        transformation = Matrix4(Matrix4d::from(node.matrix.data()));
    }

    /* Node is a camera */
    if(node.camera >= 0) {
        const UnsignedInt cameraId = node.camera;

        return std::unique_ptr<ObjectData3D>{new ObjectData3D{children, transformation, ObjectInstanceType3D::Camera, cameraId, &node}};

    /* Node is a mesh */
    } else if(node.mesh >= 0) {
        const UnsignedInt meshId = node.mesh;
        /* TODO Multi-material models not supported */
        const Int materialId = _d->model.meshes[meshId].primitives.empty() ? -1 : _d->model.meshes[meshId].primitives[0].material;

        return std::unique_ptr<ObjectData3D>{new MeshObjectData3D{children, transformation, meshId, materialId, &node}};

    /* Node is a light */
    } else if(node.extensions.find("KHR_lights_cmn") != node.extensions.end()) {
        tinygltf::Value lightValue = node.extensions.at("KHR_lights_cmn").Get("light");
        const UnsignedInt lightId = UnsignedInt(lightValue.Get<int>());

        return std::unique_ptr<ObjectData3D>{new ObjectData3D{children, transformation, ObjectInstanceType3D::Light, lightId, &node}};
    }

    return std::unique_ptr<ObjectData3D>{new ObjectData3D{children, transformation, &node}};
}

UnsignedInt TinyGltfImporter::doMesh3DCount() const {
    return _d->model.meshes.size();
}

Containers::Optional<MeshData3D> TinyGltfImporter::doMesh3D(const UnsignedInt id) {
    const tinygltf::Mesh& mesh = _d->model.meshes[id];

    /* primitive */
    if(mesh.primitives.size() > 1) {
        Error() << "Trade::TinyGltfImporter::mesh3D(): currently only one primitive per mesh is supported";
        return Containers::NullOpt;
    }

    const tinygltf::Primitive& primitive = mesh.primitives[0];

    MeshPrimitive meshPrimitive{};
    if(primitive.mode == TINYGLTF_MODE_POINTS) {
        meshPrimitive = MeshPrimitive::Points;
    } else if(primitive.mode == TINYGLTF_MODE_LINE) {
        meshPrimitive = MeshPrimitive::Lines;
    } else if(primitive.mode == TINYGLTF_MODE_LINE_LOOP) {
        meshPrimitive = MeshPrimitive::LineLoop;
    } else if(primitive.mode == 3) {
        /* For some reason tiny_gltf doesn't have a define for this */
        meshPrimitive = MeshPrimitive::LineStrip;
    } else if(primitive.mode == TINYGLTF_MODE_TRIANGLES) {
        meshPrimitive = MeshPrimitive::Triangles;
    } else if(primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
        meshPrimitive = MeshPrimitive::TriangleFan;
    } else if(primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
        meshPrimitive = MeshPrimitive::TriangleStrip;
    /* LCOV_EXCL_START */
    } else {
        Error{} << "Trade::TinyGltfImporter::mesh3D(): unrecognized primitive" << primitive.mode;
        return Containers::NullOpt;
    }
    /* LCOV_EXCL_STOP */

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<std::vector<Vector2>> textureLayers;
    std::vector<std::vector<Color4>> colorLayers;

    for(auto& attribute: primitive.attributes) {
        const tinygltf::Accessor& accessor = _d->model.accessors[attribute.second];
        const tinygltf::BufferView& bufferView = _d->model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = _d->model.buffers[bufferView.buffer];

        if(bufferView.byteStride != 0) {
            Error() << "Trade::TinyGltfImporter::mesh3D(): non-zero strides for buffer views are not supported";
            return  Containers::NullOpt;
        }

        if(attribute.first == "POSITION") {
            if(accessor.type != TINYGLTF_TYPE_VEC3) {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC3";
                return Containers::NullOpt;
            }

            positions.reserve(accessor.count);
            std::copy_n(reinterpret_cast<const Vector3*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset), accessor.count, std::back_inserter(positions));

        } else if(attribute.first == "NORMAL") {
            if(accessor.type != TINYGLTF_TYPE_VEC3) {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC3";
                return Containers::NullOpt;
            }

            normals.reserve(accessor.count);
            std::copy_n(reinterpret_cast<const Vector3*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset), accessor.count, std::back_inserter(normals));

        /* Texture coordinate attribute ends with _0, _1 ... */
        } else if(Utility::String::beginsWith(attribute.first, "TEXCOORD")) {
            if(accessor.type != TINYGLTF_TYPE_VEC2) {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC2";
                return Containers::NullOpt;
            }

            textureLayers.emplace_back();
            std::vector<Vector2>& textureCoordinates = textureLayers.back();

            textureCoordinates.reserve(accessor.count);
            std::copy_n(reinterpret_cast<const Vector2*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset), accessor.count, std::back_inserter(textureCoordinates));

        /* Color attribute ends with _0, _1 ... */
        } else if(Utility::String::beginsWith(attribute.first, "COLOR")) {
            std::vector<Color4> colors{};

            if(accessor.type == TINYGLTF_TYPE_VEC3) {
                colors.reserve(accessor.count);
                std::copy_n(reinterpret_cast<const Vector3*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset), accessor.count, std::back_inserter(colors));

            } else if(accessor.type == TINYGLTF_TYPE_VEC4) {
                colors.reserve(accessor.count);
                std::copy_n(reinterpret_cast<const Vector4*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset), accessor.count, std::back_inserter(colors));

            } else {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC3 or VEC4";
                return Containers::NullOpt;
            }

            colorLayers.emplace_back(std::move(colors));

        } else {
            Warning() << "Trade::TinyGltfImporter::mesh3D(): unsupported mesh vertex attribute" << attribute.first;
            continue;
        }
    }

    /* Indices */
    const tinygltf::Accessor& idxAccessor = _d->model.accessors[primitive.indices];
    const tinygltf::BufferView& idxBufferView = _d->model.bufferViews[idxAccessor.bufferView];
    const tinygltf::Buffer& idxBuffer = _d->model.buffers[idxBufferView.buffer];

    if(idxAccessor.type != TINYGLTF_TYPE_SCALAR) {
        Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of index is SCALAR";
        return Containers::NullOpt;
    }

    std::vector<UnsignedInt> indices;
    const UnsignedByte* start = idxBuffer.data.data() + idxBufferView.byteOffset + idxAccessor.byteOffset;
    if(idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        std::copy_n(start, idxAccessor.count, std::back_inserter(indices));
    } else if(idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        std::copy_n(reinterpret_cast<const UnsignedShort*>(start), idxAccessor.count, std::back_inserter(indices));
    } else if(idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        std::copy_n(reinterpret_cast<const UnsignedInt*>(start), idxAccessor.count, std::back_inserter(indices));
    } else CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Flip Y axis of texture coordinates */
    for(std::vector<Vector2>& layer : textureLayers) for(Vector2& c : layer) c.y() = 1.0f - c.y();

    return MeshData3D(meshPrimitive, std::move(indices), {std::move(positions)}, {std::move(normals)}, textureLayers, colorLayers, &mesh);
}

UnsignedInt TinyGltfImporter::doMaterialCount() const {
    return _d->model.materials.size();
}

Int TinyGltfImporter::doMaterialForName(const std::string& name){
    const auto found = _d->materialsForName.find(name);
    return found == _d->materialsForName.end() ? -1 : found->second;
}

std::string TinyGltfImporter::doMaterialName(const UnsignedInt id) {
    return _d->model.materials[id].name;
}

std::unique_ptr<AbstractMaterialData> TinyGltfImporter::doMaterial(const UnsignedInt id) {
    const tinygltf::Material& material = _d->model.materials[id];

    /* Textures */
    PhongMaterialData::Flags flags;
    UnsignedInt diffuseTexture{}, specularTexture{};
    Vector4 diffuseColor{1.0f};
    Vector3 specularColor{0.0f};
    Float shininess{1.0f};

    if(material.extensions.find("KHR_materials_cmnBlinnPhong") != material.extensions.end()) {
        tinygltf::Value cmnBlinnPhongExt = material.extensions.at("KHR_materials_cmnBlinnPhong");

        auto diffuseTextureValue = cmnBlinnPhongExt.Get("diffuseTexture");
        if(diffuseTextureValue.Type() != tinygltf::NULL_TYPE) {
            diffuseTexture = UnsignedInt(diffuseTextureValue.Get("index").Get<int>());
            flags |= PhongMaterialData::Flag::DiffuseTexture;
        }

        auto specularTextureValue = cmnBlinnPhongExt.Get("specularShininessTexture");
        if(specularTextureValue.Type() != tinygltf::NULL_TYPE) {
            specularTexture = UnsignedInt(specularTextureValue.Get("index").Get<int>());
            flags |= PhongMaterialData::Flag::SpecularTexture;
        }

        /* Colors */
        auto diffuseFactorValue = cmnBlinnPhongExt.Get("diffuseFactor");
        if(diffuseFactorValue.Type() != tinygltf::NULL_TYPE) {
            diffuseColor = Vector4{Vector4d{
                diffuseFactorValue.Get(0).Get<double>(),
                diffuseFactorValue.Get(1).Get<double>(),
                diffuseFactorValue.Get(2).Get<double>(),
                diffuseFactorValue.Get(3).Get<double>()}};
        }

        auto specularColorValue = cmnBlinnPhongExt.Get("specularFactor");
        if(specularColorValue.Type() != tinygltf::NULL_TYPE) {
            specularColor = Vector3{Vector3d{
                specularColorValue.Get(0).Get<double>(),
                specularColorValue.Get(1).Get<double>(),
                specularColorValue.Get(2).Get<double>()}};
        }

        /* Parameters */
        auto shininessFactorValue = cmnBlinnPhongExt.Get("shininessFactor");
        if(shininessFactorValue.Type() != tinygltf::NULL_TYPE) {
            shininess = float(shininessFactorValue.Get<double>());
        }
    } else {
        auto dt = material.values.find("baseColorTexture");
        if(dt != material.values.end()) {
            diffuseTexture = dt->second.TextureIndex();
            flags |= PhongMaterialData::Flag::DiffuseTexture;
        }

        auto baseColorFactorValue = material.values.find("baseColorFactor");
        if(baseColorFactorValue != material.values.end()) {
            tinygltf::ColorValue color = baseColorFactorValue->second.ColorFactor();
            diffuseColor = Vector4{Vector4d::from(color.data())};
        }
    }

    /* Put things together */
    std::unique_ptr<PhongMaterialData> data{new PhongMaterialData{flags, shininess, &material}};
    data->ambientColor() = Color3{0.0f};
    if(flags & PhongMaterialData::Flag::DiffuseTexture)
        data->diffuseTexture() = diffuseTexture;
    else data->diffuseColor() = diffuseColor.xyz();
    if(flags & PhongMaterialData::Flag::SpecularTexture)
        data->specularTexture() = specularTexture;
    else data->specularColor() = specularColor;
    return std::move(data);
}

UnsignedInt TinyGltfImporter::doTextureCount() const {
    return _d->model.textures.size();
}

Containers::Optional<TextureData> TinyGltfImporter::doTexture(const UnsignedInt id) {
    const tinygltf::Texture& tex = _d->model.textures[id];

    if(tex.sampler < 0) {
        /* The specification instructs to use "auto sampling", i.e. it is left to the implementor to decide on the default values... */
        return TextureData{TextureData::Type::Texture2D, SamplerFilter::Linear, SamplerFilter::Linear,
            Sampler::Mipmap::Linear, {Sampler::Wrapping::Repeat, Sampler::Wrapping::Repeat, Sampler::Wrapping::Repeat}, UnsignedInt(tex.source), &tex};
    }
    const tinygltf::Sampler& s = _d->model.samplers[tex.sampler];

    SamplerFilter minFilter;
    SamplerMipmap mipmap;
    switch(s.minFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            minFilter = SamplerFilter::Nearest;
            mipmap = SamplerMipmap::Base;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
            minFilter = SamplerFilter::Linear;
            mipmap = SamplerMipmap::Base;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            minFilter = SamplerFilter::Nearest;
            mipmap = SamplerMipmap::Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            minFilter = SamplerFilter::Nearest;
            mipmap = SamplerMipmap::Linear;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            minFilter = SamplerFilter::Linear;
            mipmap = SamplerMipmap::Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            minFilter = SamplerFilter::Linear;
            mipmap = SamplerMipmap::Linear;
            break;
        default: CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    SamplerFilter magFilter;
    switch(s.magFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            magFilter = SamplerFilter::Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
            magFilter = SamplerFilter::Linear;
            break;
        default: CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    /* There's wrapR that is a glTF extension and is set to zero. Ignoring that
       one and hardcoding it to Repeat. */
    Array3D<SamplerWrapping> wrapping;
    wrapping.z() = SamplerWrapping::Repeat;
    for(auto&& wrap: std::initializer_list<std::pair<int, int>>{
        {s.wrapS, 0}, {s.wrapT, 1}})
    {
        switch(wrap.first) {
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                wrapping[wrap.second] = SamplerWrapping::Repeat;
                break;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                wrapping[wrap.second] = SamplerWrapping::ClampToEdge;
                break;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                wrapping[wrap.second] = SamplerWrapping::MirroredRepeat;
                break;
            default: CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
    }

    /* glTF supports only 2D textures */
    return TextureData{TextureData::Type::Texture2D, minFilter, magFilter,
        mipmap, wrapping, UnsignedInt(tex.source), &tex};
}

UnsignedInt TinyGltfImporter::doImage2DCount() const {
    return _d->model.images.size();
}

Containers::Optional<ImageData2D> TinyGltfImporter::doImage2D(const UnsignedInt id) {
    CORRADE_ASSERT(manager(), "Trade::TinyGltfImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to load images", {});

    /* Because we specified an empty callback for loading image data,
       Image.image, Image.width, Image.height and Image.component will not be
       valid and should not be accessed. */

    const tinygltf::Image& image = _d->model.images[id];

    /* Load embedded image */
    if(image.uri.empty()) {
        /* @todo Use AnyImageImporter once it supports openData */
        StbImageImporter imageImporter;

        const tinygltf::BufferView& bufferView = _d->model.bufferViews[image.bufferView];
        const tinygltf::Buffer& buffer = _d->model.buffers[bufferView.buffer];

        Containers::ArrayView<const char> data = Containers::arrayCast<const char>(Containers::arrayView(&buffer.data[bufferView.byteOffset], bufferView.byteLength));
        if(!imageImporter.openData(data)) return Containers::NullOpt;

        return imageImporter.image2D(0);

    /* Load external image */
    } else {
        AnyImageImporter imageImporter{*manager()};

        const std::string filepath = Utility::Directory::join(_d->filePath, image.uri);
        if(!imageImporter.openFile(filepath)) return Containers::NullOpt;

        return imageImporter.image2D(0);
    }
}

}}

CORRADE_PLUGIN_REGISTER(TinyGltfImporter, Magnum::Trade::TinyGltfImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3")
