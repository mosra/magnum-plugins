/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>
    Copyright © 2018 Jonathan Hale <squareys@googlemail.com>

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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/String.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/CubicHermite.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>

#include "MagnumPlugins/AnyImageImporter/AnyImageImporter.h"

#define TINYGLTF_IMPLEMENTATION
/* Opt out of tinygltf stb_image dependency */
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
/* Opt out of loading external images */
#define TINYGLTF_NO_EXTERNAL_IMAGE
/* Opt out of filesystem access, as we handle it ourselves. However that makes
   it fail to compile as std::ofstream is not define, so we do that here (and
   newer versions don't seem to fix that either). Enabling filesystem access
   makes the compilation fail on WinRT 2015 because for some reason
   ExpandEnvironmentStringsA() is not defined there. Not sure what is that
   related to since windows.h is included, I assume it's related to the MSVC
   2015 bug where a duplicated templated alias causes the compiler to forget
   random symbols, but I couldn't find anything like that in the codebase.
   Also, this didn't happen before when plugins were built with GL enabled. */
#define TINYGLTF_NO_FS
#include <fstream>

#ifdef CORRADE_TARGET_WINDOWS
/* Tinygltf includes some windows headers, avoid including more than ncessary
   to speed up compilation. WIN32_LEAN_AND_MEAN and NOMINMAX is already defined
   by CMake. */
#define VC_EXTRALEAN
#endif

/* Include like this instead of "MagnumExternal/TinyGltf/tiny_gltf.h" so we can
   include it as a system header and suppress warnings */
#include "tiny_gltf.h"
#ifdef CORRADE_TARGET_WINDOWS
#undef near
#undef far
#endif

namespace Magnum { namespace Trade {

using namespace Magnum::Math::Literals;

namespace {

bool loadImageData(tinygltf::Image* image, std::string*, std::string*, int, int, const unsigned char* data, int size, void*) {
    /* In case the image is an embedded URI, copy its decoded value to the data
       buffer. In all other cases we'll access the referenced buffer or
       external file directly from the doImage2D() implementation. */
    if(image->bufferView == -1 && image->uri.empty())
        image->image.assign(data, data + size);

    return true;
}

std::size_t elementSize(const tinygltf::Accessor& accessor) {
    /* GetTypeSizeInBytes() is totally bogus and misleading name, it should
       have been called GetTypeComponentCount but who am I to judge. */
    return tinygltf::GetComponentSizeInBytes(accessor.componentType)*tinygltf::GetTypeSizeInBytes(accessor.type);
}

Containers::ArrayView<const char> bufferView(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    const std::size_t bufferElementSize = elementSize(accessor);
    CORRADE_INTERNAL_ASSERT(std::size_t(accessor.bufferView) < model.bufferViews.size());
    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    CORRADE_INTERNAL_ASSERT(std::size_t(bufferView.buffer) < model.buffers.size());
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    CORRADE_INTERNAL_ASSERT(bufferView.byteStride == 0 || bufferView.byteStride == bufferElementSize);
    return {reinterpret_cast<const char*>(buffer.data.data()) + bufferView.byteOffset + accessor.byteOffset, accessor.count*bufferElementSize};
}

template<class T> Containers::ArrayView<const T> bufferView(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    CORRADE_INTERNAL_ASSERT(elementSize(accessor) == sizeof(T));
    return Containers::arrayCast<const T>(bufferView(model, accessor));
}

}

struct TinyGltfImporter::Document {
    Containers::Optional<std::string> filePath;

    tinygltf::Model model;

    Containers::Optional<std::unordered_map<std::string, Int>>
        animationsForName,
        camerasForName,
        lightsForName,
        scenesForName,
        nodesForName,
        meshesForName,
        materialsForName,
        imagesForName,
        texturesForName;

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
    std::vector<std::pair<std::size_t, std::size_t>> meshMap;
    std::vector<std::size_t> meshSizeOffsets;

    /* Mapping for nodes having multi-primitive nodes. The same as above, but
       for nodes. Hierarchy-wise, the subsequent nodes are direct children of
       the first, have no transformation or other children and point to the
       subsequent meshes. */
    std::vector<std::pair<std::size_t, std::size_t>> nodeMap;
    std::vector<std::size_t> nodeSizeOffsets;

    bool open = false;
};

namespace {

void fillDefaultConfiguration(Utility::ConfigurationGroup& conf) {
    /** @todo horrible workaround, fix this properly */
    conf.setValue("optimizeQuaternionShortestPath", true);
    conf.setValue("normalizeQuaternions", true);
    conf.setValue("mergeAnimationClips", false);
}

}

TinyGltfImporter::TinyGltfImporter() {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

TinyGltfImporter::TinyGltfImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

TinyGltfImporter::TinyGltfImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter{manager} {
    /** @todo horrible workaround, fix this properly */
    fillDefaultConfiguration(configuration());
}

TinyGltfImporter::~TinyGltfImporter() = default;

auto TinyGltfImporter::doFeatures() const -> Features { return Feature::OpenData|Feature::FileCallback; }

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

    /* Set up file callbacks */
    tinygltf::FsCallbacks callbacks;
    callbacks.user_data = this;
    /* We don't need any expansion of environment variables in file paths.
       That should be done in a completely different place and is not something
       the importer should care about. Further, FileExists and ExpandFilePath
       is used to search for files in a few different locations. That's also
       totally useless, since location of dependent files is *clearly* and
       uniquely defined. Also, tinygltf's path joining is STUPID and so
       /foo/bar/ + /file.dat gets joined to /foo/bar//file.dat. So we supply
       an empty path there and handle it here correctly. */
    callbacks.FileExists = [](const std::string&, void*) { return true; };
    callbacks.ExpandFilePath = [](const std::string& path, void*) {
        return path;
    };
    if(fileCallback()) callbacks.ReadWholeFile = [](std::vector<unsigned char>* out, std::string* err, const std::string& filename, void* userData) {
            auto& self = *static_cast<TinyGltfImporter*>(userData);
            const std::string fullPath = Utility::Directory::join(self._d->filePath ? *self._d->filePath : "", filename);
            Containers::Optional<Containers::ArrayView<const char>> data = self.fileCallback()(fullPath, InputFileCallbackPolicy::LoadTemporary, self.fileCallbackUserData());
            if(!data) {
                *err = "file callback failed";
                return false;
            }
            out->assign(data->begin(), data->end());
            return true;
        };
    else callbacks.ReadWholeFile = [](std::vector<unsigned char>* out, std::string* err, const std::string& filename, void* userData) {
            auto& self = *static_cast<TinyGltfImporter*>(userData);
            if(!self._d->filePath) {
                *err = "external buffers can be imported only when opening files from the filesystem or if a file callback is present";
                return false;
            }
            const std::string fullPath = Utility::Directory::join(*self._d->filePath, filename);
            if(!Utility::Directory::exists(fullPath)) {
                *err = "file not found";
                return false;
            }
            Containers::Array<char> data = Utility::Directory::read(fullPath);
            out->assign(data.begin(), data.end());
            return true;
        };
    loader.SetFsCallbacks(callbacks);

    loader.SetImageLoader(&loadImageData, nullptr);

    _d->open = true;
    if(data.size() >= 4 && strncmp(data.data(), "glTF", 4) == 0) {
        _d->open = loader.LoadBinaryFromMemory(&_d->model, &err, nullptr, reinterpret_cast<const unsigned char*>(data.data()), data.size(), "", tinygltf::SectionCheck::NO_REQUIRE);
    } else {
        _d->open = loader.LoadASCIIFromString(&_d->model, &err, nullptr, data.data(), data.size(), "", tinygltf::SectionCheck::NO_REQUIRE);
    }

    if(!_d->open) {
        Utility::String::rtrimInPlace(err);
        Error{} << "Trade::TinyGltfImporter::openData(): error opening file:" << err;
        doClose();
        return;
    }

    /* Treat meshes with multiple primitives as separate meshes. Each mesh gets
       duplicated as many times as is the size of the primitives array. */
    _d->meshSizeOffsets.emplace_back(0);
    for(std::size_t i = 0; i != _d->model.meshes.size(); ++i) {
        CORRADE_INTERNAL_ASSERT(!_d->model.meshes[i].primitives.empty());
        for(std::size_t j = 0; j != _d->model.meshes[i].primitives.size(); ++j)
            _d->meshMap.emplace_back(i, j);

        _d->meshSizeOffsets.emplace_back(_d->meshMap.size());
    }

    /* In order to support multi-primitive meshes, we need to duplicate the
       nodes as well */
    _d->nodeSizeOffsets.emplace_back(0);
    for(std::size_t i = 0; i != _d->model.nodes.size(); ++i) {
        _d->nodeMap.emplace_back(i, 0);

        const Int mesh = _d->model.nodes[i].mesh;
        if(mesh != -1) {
            /* If a node has a mesh with multiple primitives, add nested nodes
               containing the other primitives after it */
            const std::size_t count = _d->model.meshes[mesh].primitives.size();
            for(std::size_t j = 1; j < count; ++j)
                _d->nodeMap.emplace_back(i, j);
        }

        _d->nodeSizeOffsets.emplace_back(_d->nodeMap.size());
    }

    /* Name maps are lazy-loaded because these might not be needed every time */
}

UnsignedInt TinyGltfImporter::doCameraCount() const {
    return _d->model.cameras.size();
}

Int TinyGltfImporter::doCameraForName(const std::string& name) {
    if(!_d->camerasForName) {
        _d->camerasForName.emplace();
        _d->camerasForName->reserve(_d->model.cameras.size());
        for(std::size_t i = 0; i != _d->model.cameras.size(); ++i)
            _d->camerasForName->emplace(_d->model.cameras[i].name, i);
    }

    const auto found = _d->camerasForName->find(name);
    return found == _d->camerasForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doCameraName(const UnsignedInt id) {
    return _d->model.cameras[id].name;
}

UnsignedInt TinyGltfImporter::doAnimationCount() const {
    /* If the animations are merged, there's at most one */
    if(configuration().value<bool>("mergeAnimationClips"))
        return _d->model.animations.empty() ? 0 : 1;

    return _d->model.animations.size();
}

Int TinyGltfImporter::doAnimationForName(const std::string& name) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return -1;

    if(!_d->animationsForName) {
        _d->animationsForName.emplace();
        _d->animationsForName->reserve(_d->model.animations.size());
        for(std::size_t i = 0; i != _d->model.animations.size(); ++i)
            _d->animationsForName->emplace(_d->model.animations[i].name, i);
    }

    const auto found = _d->animationsForName->find(name);
    return found == _d->animationsForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doAnimationName(UnsignedInt id) {
    /* If the animations are merged, don't report any names */
    if(configuration().value<bool>("mergeAnimationClips")) return {};
    return _d->model.animations[id].name;
}

namespace {

template<class V> void postprocessSplineTrack(const std::size_t timeTrackUsed, const Containers::ArrayView<const Float> keys, const Containers::ArrayView<Math::CubicHermite<V>> values) {
    /* Already processed, don't do that again */
    if(timeTrackUsed != ~std::size_t{}) return;

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

Containers::Optional<AnimationData> TinyGltfImporter::doAnimation(UnsignedInt id) {
    /* Import either a single animation or all of them together. At the moment,
       Blender doesn't really support cinematic animations (affecting multiple
       objects): https://blender.stackexchange.com/q/5689. And since
       https://github.com/KhronosGroup/glTF-Blender-Exporter/pull/166, these
       are exported as a set of object-specific clips, which may not be wanted,
       so we give the users an option to merge them all together. */
    const std::size_t animationBegin =
        configuration().value<bool>("mergeAnimationClips") ? 0 : id;
    const std::size_t animationEnd =
        configuration().value<bool>("mergeAnimationClips") ? _d->model.animations.size() : id + 1;

    /* First gather the input and output data ranges. Key is unique accessor ID
       so we don't duplicate shared data, value is range in the input buffer,
       offset in the output data and ID of the corresponding key track in case
       given track is a spline interpolation. The key ID is initialized to ~0
       and will be used later to check that a spline track was not used with
       more than one time track, as it needs to be postprocessed for given time
       track. */
    std::unordered_map<int, std::tuple<Containers::ArrayView<const char>, std::size_t, std::size_t>> samplerData;
    std::size_t dataSize = 0;
    for(std::size_t a = animationBegin; a != animationEnd; ++a) {
        const tinygltf::Animation& animation = _d->model.animations[a];
        for(std::size_t i = 0; i != animation.samplers.size(); ++i) {
            const tinygltf::AnimationSampler& sampler = animation.samplers[i];
            const tinygltf::Accessor& input = _d->model.accessors[sampler.input];
            const tinygltf::Accessor& output = _d->model.accessors[sampler.output];

            /** @todo handle alignment once we do more than just four-byte types */

            /* If the input view is not yet present in the output data buffer, add
            it */
            if(samplerData.find(sampler.input) == samplerData.end()) {
                Containers::ArrayView<const char> view = bufferView(_d->model, input);
                samplerData.emplace(sampler.input, std::make_tuple(view, dataSize, ~std::size_t{}));
                dataSize += view.size();
            }

            /* If the output view is not yet present in the output data buffer, add
            it */
            if(samplerData.find(sampler.output) == samplerData.end()) {
                Containers::ArrayView<const char> view = bufferView(_d->model, output);
                samplerData.emplace(sampler.output, std::make_tuple(view, dataSize, ~std::size_t{}));
                dataSize += view.size();
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
    for(const std::pair<int, std::tuple<Containers::ArrayView<const char>, std::size_t, std::size_t>>& view: samplerData) {
        Containers::ArrayView<const char> input;
        std::size_t outputOffset;
        std::tie(input, outputOffset, std::ignore) = view.second;

        CORRADE_INTERNAL_ASSERT(outputOffset + input.size() <= data.size());
        std::copy(input.begin(), input.end(), data.begin() + outputOffset);
    }

    /* Calculate total track count. If merging all animations together, this is
       the sum of all clip track counts. */
    std::size_t trackCount = 0;
    for(std::size_t a = animationBegin; a != animationEnd; ++a)
        trackCount += _d->model.animations[a].channels.size();

    /* Import all tracks */
    bool hadToRenormalize = false;
    std::size_t trackId = 0;
    Containers::Array<Trade::AnimationTrackData> tracks{trackCount};
    for(std::size_t a = animationBegin; a != animationEnd; ++a) {
        const tinygltf::Animation& animation = _d->model.animations[a];
        for(std::size_t i = 0; i != animation.channels.size(); ++i) {
            const tinygltf::AnimationChannel& channel = animation.channels[i];
            const tinygltf::AnimationSampler& sampler = animation.samplers[channel.sampler];

            /* Key properties -- always float time */
            const tinygltf::Accessor& input = _d->model.accessors[sampler.input];
            if(input.type != TINYGLTF_TYPE_SCALAR || input.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
                Error{} << "Trade::TinyGltfImporter::animation(): time track has unexpected type" << input.type << Debug::nospace << "/" << Debug::nospace << input.componentType;
                return Containers::NullOpt;
            }

            /* View on the key data */
            const auto inputDataFound = samplerData.find(sampler.input);
            CORRADE_INTERNAL_ASSERT(inputDataFound != samplerData.end());
            const auto keys = Containers::arrayCast<const Float>(data.suffix(std::get<1>(inputDataFound->second)).prefix(std::get<0>(inputDataFound->second).size()));

            /* Interpolation mode */
            Animation::Interpolation interpolation;
            if(sampler.interpolation == "LINEAR") {
                interpolation = Animation::Interpolation::Linear;
            } else if(sampler.interpolation == "CUBICSPLINE") {
                interpolation = Animation::Interpolation::Spline;
            } else if(sampler.interpolation == "STEP") {
                interpolation = Animation::Interpolation::Constant;
            } else {
                Error{} << "Trade::TinyGltfImporter::animation(): unsupported interpolation" << sampler.interpolation;
                return Containers::NullOpt;
            }

            /* Decide on value properties */
            const tinygltf::Accessor& output = _d->model.accessors[sampler.output];
            AnimationTrackTargetType target;
            AnimationTrackType type, resultType;
            Animation::TrackViewStorage<Float> track;
            const auto outputDataFound = samplerData.find(sampler.output);
            CORRADE_INTERNAL_ASSERT(outputDataFound != samplerData.end());
            const auto outputData = data.suffix(std::get<1>(outputDataFound->second)).prefix(std::get<0>(outputDataFound->second).size());
            std::size_t& timeTrackUsed = std::get<2>(outputDataFound->second);

            /* Translation */
            if(channel.target_path == "translation") {
                if(output.type != TINYGLTF_TYPE_VEC3 || output.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    Error{} << "Trade::TinyGltfImporter::animation(): translation track has unexpected type" << output.type << Debug::nospace << "/" << Debug::nospace << output.componentType;
                    return Containers::NullOpt;
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Translation3D;
                resultType = AnimationTrackType::Vector3;
                if(interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once for
                    every track -- postprocessSplineTrack() checks that. */
                    const auto values = Containers::arrayCast<CubicHermite3D>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermite3D;
                    track = Animation::TrackView<Float, CubicHermite3D>{
                        keys, values, interpolation,
                        animationInterpolatorFor<CubicHermite3D>(interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    type = AnimationTrackType::Vector3;
                    track = Animation::TrackView<Float, Vector3>{keys,
                        Containers::arrayCast<const Vector3>(outputData),
                        interpolation,
                        animationInterpolatorFor<Vector3>(interpolation),
                        Animation::Extrapolation::Constant};
                }

            /* Rotation */
            } else if(channel.target_path == "rotation") {
                /** @todo rotation can be also normalized (?!) to a vector of 8/16/32bit (signed?!) integers */

                if(output.type != TINYGLTF_TYPE_VEC4 || output.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    Error{} << "Trade::TinyGltfImporter::animation(): rotation track has unexpected type" << output.type << Debug::nospace << "/" << Debug::nospace << output.componentType;
                    return Containers::NullOpt;
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Rotation3D;
                resultType = AnimationTrackType::Quaternion;
                if(interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once for
                    every track -- postprocessSplineTrack() checks that. */
                    const auto values = Containers::arrayCast<CubicHermiteQuaternion>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermiteQuaternion;
                    track = Animation::TrackView<Float, CubicHermiteQuaternion>{
                        keys, values, interpolation,
                        animationInterpolatorFor<CubicHermiteQuaternion>(interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    /* Ensure shortest path is always chosen. Not doing this for
                    spline interpolation, there it would cause war and famine. */
                    const auto values = Containers::arrayCast<Quaternion>(outputData);
                    if(configuration().value<bool>("optimizeQuaternionShortestPath")) {
                        Float flip = 1.0f;
                        for(std::size_t i = 0; i != values.size() - 1; ++i) {
                            if(Math::dot(values[i], values[i + 1]*flip) < 0) flip = -flip;
                            values[i + 1] *= flip;
                        }
                    }

                    /* Normalize the quaternions if not already. Don't attempt to
                    normalize every time to avoid tiny differences, only when
                    the quaternion looks to be off. Again, not doing this for
                    splines as it would cause things to go haywire. */
                    if(configuration().value<bool>("normalizeQuaternions")) {
                        for(auto& i: values) if(!i.isNormalized()) {
                            i = i.normalized();
                            hadToRenormalize = true;
                        }
                    }

                    type = AnimationTrackType::Quaternion;
                    track = Animation::TrackView<Float, Quaternion>{
                        keys, values, interpolation,
                        animationInterpolatorFor<Quaternion>(interpolation),
                        Animation::Extrapolation::Constant};
                }

            /* Scale */
            } else if(channel.target_path == "scale") {
                if(output.type != TINYGLTF_TYPE_VEC3 || output.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    Error{} << "Trade::TinyGltfImporter::animation(): scaling track has unexpected type" << output.type << Debug::nospace << "/" << Debug::nospace << output.componentType;
                    return Containers::NullOpt;
                }

                /* View on the value data */
                target = AnimationTrackTargetType::Scaling3D;
                resultType = AnimationTrackType::Vector3;
                if(interpolation == Animation::Interpolation::Spline) {
                    /* Postprocess the spline track. This can be done only once for
                    every track -- postprocessSplineTrack() checks that. */
                    const auto values = Containers::arrayCast<CubicHermite3D>(outputData);
                    postprocessSplineTrack(timeTrackUsed, keys, values);

                    type = AnimationTrackType::CubicHermite3D;
                    track = Animation::TrackView<Float, CubicHermite3D>{
                        keys, values, interpolation,
                        animationInterpolatorFor<CubicHermite3D>(interpolation),
                        Animation::Extrapolation::Constant};
                } else {
                    type = AnimationTrackType::Vector3;
                    track = Animation::TrackView<Float, Vector3>{keys,
                        Containers::arrayCast<const Vector3>(outputData),
                        interpolation,
                        animationInterpolatorFor<Vector3>(interpolation),
                        Animation::Extrapolation::Constant};
                }

            } else {
                Error{} << "Trade::TinyGltfImporter::animation(): unsupported track target" << channel.target_path;
                return Containers::NullOpt;
            }

            /* Splines were postprocessed using the corresponding time track. If a
            spline is not yet marked as postprocessed, mark it. Otherwise check
            that the spline track is always used with the same time track. */
            if(interpolation == Animation::Interpolation::Spline) {
                if(timeTrackUsed == ~std::size_t{})
                    timeTrackUsed = sampler.input;
                else if(timeTrackUsed != std::size_t(sampler.input)) {
                    Error{} << "Trade::TinyGltfImporter::animation(): spline track is shared with different time tracks, we don't support that, sorry";
                    return Containers::NullOpt;
                }
            }

            tracks[trackId++] = AnimationTrackData{type, resultType, target,
                /* In cases where multi-primitive mesh nodes are split into multipe
                objects, the animation should affect the first node -- the other
                nodes are direct children of it and so they get affected too */
                UnsignedInt(_d->nodeSizeOffsets[channel.target_node]),
                track};
        }
    }

    if(hadToRenormalize)
        Warning{} << "Trade::TinyGltfImporter::animation(): quaternions in some rotation tracks were renormalized";

    return AnimationData{std::move(data), std::move(tracks),
        configuration().value<bool>("mergeAnimationClips") ? nullptr :
        &_d->model.animations[id]};
}

Containers::Optional<CameraData> TinyGltfImporter::doCamera(UnsignedInt id) {
    const tinygltf::Camera& camera = _d->model.cameras[id];

    /* https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#projection-matrices */

    /* Perspective camera. glTF uses vertical FoV and X/Y aspect ratio, so to
       avoid accidental bugs we will directly calculate the near plane size and
       use that to create the camera data (instead of passing it the horizontal
       FoV). Also tinygltf is stupid and uses 0 to denote infinite far plane
       (wat). */
    if(camera.type == "perspective") {
        const Vector2 size = 2.0f*camera.perspective.znear*Math::tan(camera.perspective.yfov*0.5_radf)*Vector2::xScale(camera.perspective.aspectRatio);
        const Float far = camera.perspective.zfar == 0.0f ? Constants::inf() :
            camera.perspective.zfar;
        return CameraData{CameraType::Perspective3D, size, camera.perspective.znear, far, &camera};
    }

    /* Orthographic camera. glTF uses a "scale" instead of "size", which means
       we have to double. */
    if(camera.type == "orthographic")
        return CameraData{CameraType::Orthographic3D,
            Vector2{camera.orthographic.xmag, camera.orthographic.ymag}*2.0f,
            camera.orthographic.znear, camera.orthographic.zfar, &camera};

    CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

UnsignedInt TinyGltfImporter::doLightCount() const {
    return _d->model.lights.size();
}

Int TinyGltfImporter::doLightForName(const std::string& name) {
    if(!_d->lightsForName) {
        _d->lightsForName.emplace();
        _d->lightsForName->reserve(_d->model.lights.size());
        for(std::size_t i = 0; i != _d->model.lights.size(); ++i)
            _d->lightsForName->emplace(_d->model.lights[i].name, i);
    }

    const auto found = _d->lightsForName->find(name);
    return found == _d->lightsForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doLightName(const UnsignedInt id) {
    return _d->model.lights[id].name;
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

Int TinyGltfImporter::doDefaultScene() {
    /* While https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#scenes
       says that "When scene is undefined, runtime is not required to render
       anything at load time.", several official sample glTF models (e.g. the
       AnimatedTriangle) have no "scene" property, so that's a bit stupid
       behavior to have. As per discussion at https://github.com/KhronosGroup/glTF/issues/815#issuecomment-274286889,
       if a default scene isn't defined and there is at least one scene, just
       use the first one. */
    if(_d->model.defaultScene == -1 && !_d->model.scenes.empty())
        return 0;

    return _d->model.defaultScene;
}

UnsignedInt TinyGltfImporter::doSceneCount() const { return _d->model.scenes.size(); }

Int TinyGltfImporter::doSceneForName(const std::string& name) {
    if(!_d->scenesForName) {
        _d->scenesForName.emplace();
        _d->scenesForName->reserve(_d->model.scenes.size());
        for(std::size_t i = 0; i != _d->model.scenes.size(); ++i)
            _d->scenesForName->emplace(_d->model.scenes[i].name, i);
    }

    const auto found = _d->scenesForName->find(name);
    return found == _d->scenesForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doSceneName(const UnsignedInt id) {
    return _d->model.scenes[id].name;
}

Containers::Optional<SceneData> TinyGltfImporter::doScene(UnsignedInt id) {
    const tinygltf::Scene& scene = _d->model.scenes[id];

    /* The scene contains always the top-level nodes, all multi-primitive mesh
       nodes are children of them */
    std::vector<UnsignedInt> children;
    children.reserve(scene.nodes.size());
    for(const std::size_t i: scene.nodes)
        children.push_back(_d->nodeSizeOffsets[i]);

    return SceneData{{}, std::move(children), &scene};
}

UnsignedInt TinyGltfImporter::doObject3DCount() const {
    return _d->nodeMap.size();
}

Int TinyGltfImporter::doObject3DForName(const std::string& name) {
    if(!_d->nodesForName) {
        _d->nodesForName.emplace();
        _d->nodesForName->reserve(_d->model.nodes.size());
        for(std::size_t i = 0; i != _d->model.nodes.size(); ++i) {
            /* A mesh node can be duplicated for as many primitives as the mesh
               has, point to the first node in the duplicate sequence */
            _d->nodesForName->emplace(_d->model.nodes[i].name, _d->nodeSizeOffsets[i]);
        }
    }

    const auto found = _d->nodesForName->find(name);
    return found == _d->nodesForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doObject3DName(UnsignedInt id) {
    /* This returns the same name for all multi-primitive mesh node duplicates */
    return _d->model.nodes[_d->nodeMap[id].first].name;
}

Containers::Pointer<ObjectData3D> TinyGltfImporter::doObject3D(UnsignedInt id) {
    const std::size_t originalNodeId = _d->nodeMap[id].first;
    const tinygltf::Node& node = _d->model.nodes[originalNodeId];

    /* This is an extra node added for multi-primitive meshes -- return it with
       no children, identity transformation and just a link to the particular
       mesh & material combo */
    const std::size_t nodePrimitiveId = _d->nodeMap[id].second;
    if(nodePrimitiveId) {
        const UnsignedInt meshId = _d->meshSizeOffsets[node.mesh] + nodePrimitiveId;
        const Int materialId = _d->model.meshes[node.mesh].primitives[nodePrimitiveId].material;
        return Containers::pointer(new MeshObjectData3D{{}, {}, {}, Vector3{1.0f}, meshId, materialId, &node});
    }

    CORRADE_INTERNAL_ASSERT(node.rotation.size() == 0 || node.rotation.size() == 4);
    CORRADE_INTERNAL_ASSERT(node.translation.size() == 0 || node.translation.size() == 3);
    CORRADE_INTERNAL_ASSERT(node.scale.size() == 0 || node.scale.size() == 3);
    /* Ensure we have either a matrix or T-R-S */
    CORRADE_INTERNAL_ASSERT(node.matrix.size() == 0 ||
        (node.matrix.size() == 16 && node.translation.size() == 0 && node.rotation.size() == 0 && node.scale.size() == 0));

    /* Node children: first add extra nodes caused by multi-primitive meshes,
       after that the usual children. */
    std::vector<UnsignedInt> children;
    const std::size_t extraChildrenCount = _d->nodeSizeOffsets[originalNodeId + 1] - _d->nodeSizeOffsets[originalNodeId] - 1;
    children.reserve(extraChildrenCount + node.children.size());
    for(std::size_t i = 0; i != extraChildrenCount; ++i) {
        /** @todo the test should fail with children.push_back(originalNodeId + i + 1); */
        children.push_back(_d->nodeSizeOffsets[originalNodeId] + i + 1);
    }
    for(const std::size_t i: node.children)
        children.push_back(_d->nodeSizeOffsets[i]);

    /* According to the spec, order is T-R-S: first scale, then rotate, then
       translate (or translate*rotate*scale multiplication of matrices). Makes
       most sense, since non-uniform scaling of rotated object is unwanted in
       99% cases, similarly with rotating or scaling a translated object. Also
       independently verified by exporting a model with translation, rotation
       *and* scaling of hierarchic objects. */
    ObjectFlags3D flags;
    Matrix4 transformation;
    Vector3 translation;
    Quaternion rotation;
    Vector3 scaling{1.0f};
    if(node.matrix.size() == 16) {
        transformation = Matrix4(Matrix4d::from(node.matrix.data()));
    } else {
        /* Having TRS is a better property than not having it, so we set this
           flag even when there is no transformation at all. */
        flags |= ObjectFlag3D::HasTranslationRotationScaling;
        if(node.translation.size() == 3)
            translation = Vector3{Vector3d::from(node.translation.data())};
        if(node.rotation.size() == 4) {
            rotation = Quaternion{Vector3{Vector3d::from(node.rotation.data())}, Float(node.rotation[3])};
            if(!rotation.isNormalized() && configuration().value<bool>("normalizeQuaternions")) {
                rotation = rotation.normalized();
                Warning{} << "Trade::TinyGltfImporter::object3D(): rotation quaternion was renormalized";
            }
        }
        if(node.scale.size() == 3)
            scaling = Vector3{Vector3d::from(node.scale.data())};
    }

    /* Node is a mesh */
    if(node.mesh >= 0) {
        /* Multi-primitive nodes are handled above */
        CORRADE_INTERNAL_ASSERT(_d->nodeMap[id].second == 0);
        CORRADE_INTERNAL_ASSERT(!_d->model.meshes[node.mesh].primitives.empty());

        const UnsignedInt meshId = _d->meshSizeOffsets[node.mesh];
        const Int materialId = _d->model.meshes[node.mesh].primitives[0].material;
        return Containers::pointer(flags & ObjectFlag3D::HasTranslationRotationScaling ?
            new MeshObjectData3D{std::move(children), translation, rotation, scaling, meshId, materialId, &node} :
            new MeshObjectData3D{std::move(children), transformation, meshId, materialId, &node});
    }

    /* Unknown nodes are treated as Empty */
    ObjectInstanceType3D instanceType = ObjectInstanceType3D::Empty;
    UnsignedInt instanceId = ~UnsignedInt{}; /* -1 */

    /* Node is a camera */
    if(node.camera >= 0) {
        instanceType = ObjectInstanceType3D::Camera;
        instanceId = node.camera;

    /* Node is a light */
    } else if(node.extensions.find("KHR_lights_cmn") != node.extensions.end()) {
        instanceType = ObjectInstanceType3D::Light;
        instanceId = UnsignedInt(node.extensions.at("KHR_lights_cmn").Get("light").Get<int>());
    }

    return Containers::pointer(flags & ObjectFlag3D::HasTranslationRotationScaling ?
        new ObjectData3D{std::move(children), translation, rotation, scaling, instanceType, instanceId, &node} :
        new ObjectData3D{std::move(children), transformation, instanceType, instanceId, &node});
}

UnsignedInt TinyGltfImporter::doMesh3DCount() const {
    return _d->meshMap.size();
}

Int TinyGltfImporter::doMesh3DForName(const std::string& name) {
    if(!_d->meshesForName) {
        _d->meshesForName.emplace();
        _d->meshesForName->reserve(_d->model.meshes.size());
        for(std::size_t i = 0; i != _d->model.meshes.size(); ++i) {
            /* The mesh can be duplicated for as many primitives as it has,
               point to the first mesh in the duplicate sequence */
            _d->meshesForName->emplace(_d->model.meshes[i].name, _d->meshSizeOffsets[i]);
        }
    }

    const auto found = _d->meshesForName->find(name);
    return found == _d->meshesForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doMesh3DName(const UnsignedInt id) {
    /* This returns the same name for all multi-primitive mesh duplicates */
    return _d->model.meshes[_d->meshMap[id].first].name;
}

Containers::Optional<MeshData3D> TinyGltfImporter::doMesh3D(const UnsignedInt id) {
    const tinygltf::Mesh& mesh = _d->model.meshes[_d->meshMap[id].first];
    const tinygltf::Primitive& primitive = mesh.primitives[_d->meshMap[id].second];

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
    std::vector<std::vector<Vector3>> normalArrays;
    std::vector<std::vector<Vector2>> textureCoordinateArrays;
    std::vector<std::vector<Color4>> colorArrays;
    for(auto& attribute: primitive.attributes) {
        const tinygltf::Accessor& accessor = _d->model.accessors[attribute.second];
        const tinygltf::BufferView& view = _d->model.bufferViews[accessor.bufferView];

        /* Some of the Khronos sample models have explicitly specified stride
           (without interleaving), don't fail on that */
        if(view.byteStride != 0 && view.byteStride != elementSize(accessor)) {
            Error() << "Trade::TinyGltfImporter::mesh3D(): interleaved buffer views are not supported";
            return  Containers::NullOpt;
        }

        /* At the moment all vertex attributes should have float underlying
           type */
        if(accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
            Error() << "Trade::TinyGltfImporter::mesh3D(): vertex attribute" << attribute.first << "has unexpected type" << accessor.componentType;
            return Containers::NullOpt;
        }

        if(attribute.first == "POSITION") {
            if(accessor.type != TINYGLTF_TYPE_VEC3) {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC3";
                return Containers::NullOpt;
            }

            positions.reserve(accessor.count);
            const auto buffer = bufferView<Vector3>(_d->model, accessor);
            std::copy(buffer.begin(), buffer.end(), std::back_inserter(positions));

        } else if(attribute.first == "NORMAL") {
            if(accessor.type != TINYGLTF_TYPE_VEC3) {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC3";
                return Containers::NullOpt;
            }

            normalArrays.emplace_back();
            std::vector<Vector3>& normals = normalArrays.back();
            normals.reserve(accessor.count);
            const auto buffer = bufferView<Vector3>(_d->model, accessor);
            std::copy(buffer.begin(), buffer.end(), std::back_inserter(normals));

        /* Texture coordinate attribute ends with _0, _1 ... */
        } else if(Utility::String::beginsWith(attribute.first, "TEXCOORD")) {
            if(accessor.type != TINYGLTF_TYPE_VEC2) {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC2";
                return Containers::NullOpt;
            }

            textureCoordinateArrays.emplace_back();
            std::vector<Vector2>& textureCoordinates = textureCoordinateArrays.back();
            textureCoordinates.reserve(accessor.count);
            const auto buffer = bufferView<Vector2>(_d->model, accessor);
            std::copy(buffer.begin(), buffer.end(), std::back_inserter(textureCoordinates));

        /* Color attribute ends with _0, _1 ... */
        } else if(Utility::String::beginsWith(attribute.first, "COLOR")) {
            colorArrays.emplace_back();
            std::vector<Color4>& colors = colorArrays.back();
            colors.reserve(accessor.count);

            if(accessor.type == TINYGLTF_TYPE_VEC3) {
                colors.reserve(accessor.count);
                const auto buffer = bufferView<Color3>(_d->model, accessor);
                std::copy(buffer.begin(), buffer.end(), std::back_inserter(colors));

            } else if(accessor.type == TINYGLTF_TYPE_VEC4) {
                colors.reserve(accessor.count);
                const auto buffer = bufferView<Color4>(_d->model, accessor);
                std::copy(buffer.begin(), buffer.end(), std::back_inserter(colors));

            } else {
                Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of" << attribute.first << "is VEC3 or VEC4";
                return Containers::NullOpt;
            }

        } else {
            Warning() << "Trade::TinyGltfImporter::mesh3D(): unsupported mesh vertex attribute" << attribute.first;
            continue;
        }
    }

    /* Indices */
    std::vector<UnsignedInt> indices;
    if(primitive.indices != -1) {
        const tinygltf::Accessor& accessor = _d->model.accessors[primitive.indices];

        if(accessor.type != TINYGLTF_TYPE_SCALAR) {
            Error() << "Trade::TinyGltfImporter::mesh3D(): expected type of index is SCALAR";
            return Containers::NullOpt;
        }

        indices.reserve(accessor.count);
        if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            const auto buffer = bufferView<UnsignedByte>(_d->model, accessor);
            std::copy(buffer.begin(), buffer.end(), std::back_inserter(indices));
        } else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const auto buffer = bufferView<UnsignedShort>(_d->model, accessor);
            std::copy(buffer.begin(), buffer.end(), std::back_inserter(indices));
        } else if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const auto buffer = bufferView<UnsignedInt>(_d->model, accessor);
            std::copy(buffer.begin(), buffer.end(), std::back_inserter(indices));
        } else CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    /* Flip Y axis of texture coordinates */
    for(std::vector<Vector2>& layer: textureCoordinateArrays)
        for(Vector2& c: layer) c.y() = 1.0f - c.y();

    return MeshData3D(meshPrimitive, std::move(indices), {std::move(positions)}, std::move(normalArrays), std::move(textureCoordinateArrays), std::move(colorArrays), &mesh);
}

UnsignedInt TinyGltfImporter::doMaterialCount() const {
    return _d->model.materials.size();
}

Int TinyGltfImporter::doMaterialForName(const std::string& name) {
    if(!_d->materialsForName) {
        _d->materialsForName.emplace();
        _d->materialsForName->reserve(_d->model.materials.size());
        for(std::size_t i = 0; i != _d->model.materials.size(); ++i)
            _d->materialsForName->emplace(_d->model.materials[i].name, i);
    }

    const auto found = _d->materialsForName->find(name);
    return found == _d->materialsForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doMaterialName(const UnsignedInt id) {
    return _d->model.materials[id].name;
}

Containers::Pointer<AbstractMaterialData> TinyGltfImporter::doMaterial(const UnsignedInt id) {
    const tinygltf::Material& material = _d->model.materials[id];

    /* Alpha mode and mask, double sided */
    PhongMaterialData::Flags flags;
    MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;
    Float alphaMask = 0.5f;
    {
        auto found = material.additionalValues.find("alphaCutoff");
        if(found != material.additionalValues.end())
            alphaMask = found->second.Factor();
    } {
        auto found = material.additionalValues.find("alphaMode");
        if(found != material.additionalValues.end()) {
            if(found->second.string_value == "OPAQUE")
                alphaMode = MaterialAlphaMode::Opaque;
            else if(found->second.string_value == "BLEND")
                alphaMode = MaterialAlphaMode::Blend;
            else if(found->second.string_value == "MASK")
                alphaMode = MaterialAlphaMode::Mask;
            else {
                Error{} << "Trade::TinyGltfImporter::material(): unknown alpha mode" << found->second.string_value;
                return nullptr;
            }
        }
    } {
        auto found = material.additionalValues.find("doubleSided");
        if(found != material.additionalValues.end() && found->second.bool_value)
            flags |= PhongMaterialData::Flag::DoubleSided;
    }

    /* Textures */
    UnsignedInt diffuseTexture{}, specularTexture{};
    Color4 diffuseColor{1.0f};
    Color3 specularColor{1.0f};
    Float shininess{1.0f};

    /* Make Blinn/Phong a priority, because there we can import most properties */
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

    /* After that there is the PBR Specular/Glosiness */
    } else if(material.extensions.find("KHR_materials_pbrSpecularGlossiness") != material.extensions.end()) {
        tinygltf::Value pbrSpecularGlossiness = material.extensions.at("KHR_materials_pbrSpecularGlossiness");

        auto diffuseTextureValue = pbrSpecularGlossiness.Get("diffuseTexture");
        if(diffuseTextureValue.Type() != tinygltf::NULL_TYPE) {
            diffuseTexture = UnsignedInt(diffuseTextureValue.Get("index").Get<int>());
            flags |= PhongMaterialData::Flag::DiffuseTexture;
        }

        auto specularTextureValue = pbrSpecularGlossiness.Get("specularGlossinessTexture");
        if(specularTextureValue.Type() != tinygltf::NULL_TYPE) {
            specularTexture = UnsignedInt(specularTextureValue.Get("index").Get<int>());
            flags |= PhongMaterialData::Flag::SpecularTexture;
        }

        /* Colors */
        auto diffuseFactorValue = pbrSpecularGlossiness.Get("diffuseFactor");
        if(diffuseFactorValue.Type() != tinygltf::NULL_TYPE) {
            diffuseColor = Vector4{Vector4d{
                diffuseFactorValue.Get(0).Get<double>(),
                diffuseFactorValue.Get(1).Get<double>(),
                diffuseFactorValue.Get(2).Get<double>(),
                diffuseFactorValue.Get(3).Get<double>()}};
        }

        auto specularColorValue = pbrSpecularGlossiness.Get("specularFactor");
        if(specularColorValue.Type() != tinygltf::NULL_TYPE) {
            specularColor = Vector3{Vector3d{
                specularColorValue.Get(0).Get<double>(),
                specularColorValue.Get(1).Get<double>(),
                specularColorValue.Get(2).Get<double>()}};
        }

    /* From the core Metallic/Roughness we get just the base color / texture */
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
    Containers::Pointer<PhongMaterialData> data{Containers::InPlaceInit, flags, alphaMode, alphaMask, shininess, &material};
    if(flags & PhongMaterialData::Flag::DiffuseTexture)
        data->diffuseTexture() = diffuseTexture;
    else data->diffuseColor() = diffuseColor;
    if(flags & PhongMaterialData::Flag::SpecularTexture)
        data->specularTexture() = specularTexture;
    else data->specularColor() = specularColor;

    /* Needs std::move on GCC 4.8 and Clang 3.8 so it can properly upcast the
       pointer. */
    return std::move(data);
}

UnsignedInt TinyGltfImporter::doTextureCount() const {
    return _d->model.textures.size();
}

Int TinyGltfImporter::doTextureForName(const std::string& name) {
    if(!_d->texturesForName) {
        _d->texturesForName.emplace();
        _d->texturesForName->reserve(_d->model.textures.size());
        for(std::size_t i = 0; i != _d->model.textures.size(); ++i)
            _d->texturesForName->emplace(_d->model.textures[i].name, i);
    }

    const auto found = _d->texturesForName->find(name);
    return found == _d->texturesForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doTextureName(const UnsignedInt id) {
    return _d->model.textures[id].name;
}

Containers::Optional<TextureData> TinyGltfImporter::doTexture(const UnsignedInt id) {
    const tinygltf::Texture& tex = _d->model.textures[id];

    if(tex.sampler < 0) {
        /* The specification instructs to use "auto sampling", i.e. it is left
           to the implementor to decide on the default values... */
        return TextureData{TextureData::Type::Texture2D, SamplerFilter::Linear, SamplerFilter::Linear,
            SamplerMipmap::Linear, {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat}, UnsignedInt(tex.source), &tex};
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

    /* There's wrapR that is a tiny_gltf extension and is set to zero. Ignoring
       that one and hardcoding it to Repeat. */
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

Int TinyGltfImporter::doImage2DForName(const std::string& name) {
    if(!_d->imagesForName) {
        _d->imagesForName.emplace();
        _d->imagesForName->reserve(_d->model.images.size());
        for(std::size_t i = 0; i != _d->model.images.size(); ++i)
            _d->imagesForName->emplace(_d->model.images[i].name, i);
    }

    const auto found = _d->imagesForName->find(name);
    return found == _d->imagesForName->end() ? -1 : found->second;
}

std::string TinyGltfImporter::doImage2DName(const UnsignedInt id) {
    return _d->model.images[id].name;
}

Containers::Optional<ImageData2D> TinyGltfImporter::doImage2D(const UnsignedInt id) {
    CORRADE_ASSERT(manager(), "Trade::TinyGltfImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to load images", {});

    /* Because we specified an empty callback for loading image data,
       Image.image, Image.width, Image.height and Image.component will not be
       valid and should not be accessed. */

    const tinygltf::Image& image = _d->model.images[id];

    AnyImageImporter imageImporter{*manager()};
    if(fileCallback()) imageImporter.setFileCallback(fileCallback(), fileCallbackUserData());

    /* Load embedded image */
    if(image.uri.empty()) {
        Containers::ArrayView<const char> data;

        /* The image data are stored in a buffer */
        if(image.bufferView != -1) {
            const tinygltf::BufferView& bufferView = _d->model.bufferViews[image.bufferView];
            const tinygltf::Buffer& buffer = _d->model.buffers[bufferView.buffer];

            data = Containers::arrayCast<const char>(Containers::arrayView(&buffer.data[bufferView.byteOffset], bufferView.byteLength));

        /* Image data were a data URI, the loadImageData() callback copied them
           without decoding to the internal data vector */
        } else {
            data = Containers::arrayCast<const char>(Containers::arrayView(image.image.data(), image.image.size()));
        }

        Containers::Optional<ImageData2D> imageData;
        if(!imageImporter.openData(data) || !(imageData = imageImporter.image2D(0)))
            return Containers::NullOpt;

        return ImageData2D{std::move(*imageData), &image};

    /* Load external image */
    } else {
        if(!_d->filePath && !fileCallback()) {
            Error{} << "Trade::TinyGltfImporter::image2D(): external images can be imported only when opening files from the filesystem or if a file callback is present";
            return {};
        }

        Containers::Optional<ImageData2D> imageData;
        if(!imageImporter.openFile(Utility::Directory::join(_d->filePath ? *_d->filePath : "", image.uri)) || !(imageData = imageImporter.image2D(0)))
            return Containers::NullOpt;

        return ImageData2D{std::move(*imageData), &image};
    }
}

const void* TinyGltfImporter::doImporterState() const {
    return &_d->model;
}

}}

CORRADE_PLUGIN_REGISTER(TinyGltfImporter, Magnum::Trade::TinyGltfImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3")
