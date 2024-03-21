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

#include "UrdfImporter.h"

#include <unordered_map>
#include <Corrade/Containers/ArrayTuple.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/MurmurHash2.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Math/ConfigurationValue.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Quaternion.h>

#define PUGIXML_NO_STL // TODO this will probably cause ABI issues
#include "pugixml.hpp"

/* std::hash specialization to be able to use StringView in unordered_map.
   This needs to be some common header because GltfImporter has exactly the
   same. */
namespace std {
    // TODO isn't this in Containers already?
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

struct UrdfImporter::State {
    pugi::xml_document doc;
    Containers::Array<Containers::StringView> nodeNames;
    std::unordered_map<Containers::StringView, UnsignedInt> nodesForName;
};

UrdfImporter::UrdfImporter() = default;

UrdfImporter::UrdfImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

UrdfImporter::~UrdfImporter() = default;

ImporterFeatures UrdfImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool UrdfImporter::doIsOpened() const { return !!_in; }

void UrdfImporter::doClose() { _in = nullptr; }

void UrdfImporter::doOpenData(Containers::Array<char>&& data, DataFlags) {
    Containers::Pointer<State> state{InPlaceInit};

    const pugi::xml_parse_result result = state->doc.load_buffer(data, data.size()); // TODO options
    if(!result) {
        Error{} << "Trade::UrdfImporter::openData(): error opening file:" << result.description();
        // TODO offset, etc
        return;
    }

    /* Gather link and joint names. Use empty names for other nodes in the
       hierarchy. */
    for(const pugi::xml_node node: state->doc.root().child("robot").children()) {
        if(node.name() == "joint"_s) {
            if(const pugi::xml_attribute name = node.attribute("name"))
                arrayAppend(state->nodeNames, name.as_string());
            else
                arrayAppend(state->nodeNames, ""_s);

        } else if(node.name() == "link"_s) {
            if(const pugi::xml_attribute name = node.attribute("name"))
                arrayAppend(state->nodeNames, name.as_string());
            else
                arrayAppend(state->nodeNames, ""_s);

            if(const pugi::xml_node inertial = node.child("inertial")) {
                arrayAppend(state->nodeNames, ""_s);
            }
            if(const pugi::xml_node visual = node.child("visual")) {
                arrayAppend(state->nodeNames, ""_s);
            }
            if(const pugi::xml_node collision = node.child("collision")) {
                arrayAppend(state->nodeNames, ""_s);
            }

        } else continue;
    }

    /* Build a name->id map from these */
    for(std::size_t i = 0; i != state->nodeNames.size(); ++i)
        if(const Containers::StringView name = state->nodeNames[i])
            state->nodesForName.emplace(name, i);

    /* All good */
    _in = std::move(state);
}

Int UrdfImporter::doDefaultScene() const { return 0; }

UnsignedInt UrdfImporter::doSceneCount() const { return 1; }

Containers::String UrdfImporter::doSceneName(UnsignedInt) {
    return _in->doc.root().child("robot").attribute("name").as_string();
}

Int UrdfImporter::doSceneForName(Containers::StringView) {
    return -1; // TODO
}
namespace {

constexpr auto SceneFieldJointAxis = sceneFieldCustom(1);
constexpr auto SceneFieldJointLimitLower = sceneFieldCustom(2);
constexpr auto SceneFieldJointLimitUpper = sceneFieldCustom(3);
constexpr auto SceneFieldJointLimitEffort = sceneFieldCustom(4);
constexpr auto SceneFieldJointLimitVelocity = sceneFieldCustom(5);
constexpr auto SceneFieldJointDynamicsDamping = sceneFieldCustom(6);
constexpr auto SceneFieldJointDynamicsFriction = sceneFieldCustom(7);
constexpr auto SceneFieldLinkInertialMass = sceneFieldCustom(8);
constexpr auto SceneFieldLinkInertia = sceneFieldCustom(9);
constexpr auto SceneFieldCollisionMesh = sceneFieldCustom(10);

}

Containers::String UrdfImporter::doSceneFieldName(const UnsignedInt name) {
    switch(name) {
        // TODO lowercase names for consistency with custom material attrs etc
        #define _c(name) \
            case sceneFieldCustom(SceneField ## name): return #name;
        _c(JointAxis)
        _c(JointLimitLower)
        _c(JointLimitUpper)
        _c(JointLimitEffort)
        _c(JointLimitVelocity)
        _c(JointDynamicsDamping)
        _c(JointDynamicsFriction)
        _c(LinkInertialMass)
        _c(LinkInertia)
        _c(CollisionMesh)
        #undef _c
    }

    return {};
}

SceneField UrdfImporter::doSceneFieldForName(Containers::StringView) {
    return {}; // TODO
}

Containers::Optional<SceneData> UrdfImporter::doScene(UnsignedInt) {
    /* Gather count of joints and links */
    std::size_t parentCount = 0;
    std::size_t transformationCount = 0;

    std::size_t jointAxesCount = 0;
    std::size_t jointLimitCount = 0;
    std::size_t jointDynamicsCount = 0;

    std::size_t linkInertialCount = 0;
    std::size_t linkVisualCount = 0;
    std::size_t linkCollisionCount = 0;

    bool hasMaterials = false;
    for(const pugi::xml_node node: _in->doc.root().child("robot").children()) {
        if(node.name() == "joint"_s) {
            ++parentCount;
            if(node.child("origin")) ++transformationCount;

            if(node.child("axis")) ++jointAxesCount;
            if(node.child("dynamics")) ++jointDynamicsCount;
            if(node.child("limit")) ++jointLimitCount; // TODO check if required

        } else if(node.name() == "link"_s) {
            ++parentCount;
            if(const pugi::xml_node inertial = node.child("inertial")) {
                ++parentCount;
                ++linkInertialCount;
                if(inertial.child("origin")) ++transformationCount;
            }
            if(const pugi::xml_node visual = node.child("visual")) {
                ++parentCount;
                ++linkVisualCount;
                if(visual.child("origin")) ++transformationCount;
                if(visual.child("material")) hasMaterials = true;
            }
            if(const pugi::xml_node collision = node.child("collision")) {
                ++parentCount;
                ++linkCollisionCount; // TODO multiple?
                if(collision.child("origin")) ++transformationCount;
            }

        } else {
            Warning{} << "Trade::UrdfImporter::scene(): ignoring unknown node" << node.name();
        }
    }

//     !Debug{} << "joints" << jointCount << "axes" << jointAxesCount << "limits" << jointLimitCount << "dynamics" << jointDynamicsCount;
//     !Debug{} << "links" << linkCount << "iner" << linkInertialCount << "vs" << linkVisualCount << "col" << linkCollisionCount;

    struct Transformation {
        UnsignedInt mapping;
        Vector3 translation;
        Quaternion rotation;
    };

    struct Parent {
        UnsignedInt mapping;
        Int parent;
    };

    struct JointAxis {
        UnsignedInt mapping;
        Vector3 axis;
    };

    struct JointLimit {
        UnsignedInt mapping;
        Float lower;
        Float upper;
        Float effort;
        Float velocity;
    };

    struct JointDynamics {
        UnsignedInt mapping;
        Float damping;
        Float friction;
    };

    struct LinkInertial {
        UnsignedInt mapping;
        Float mass;
        Matrix3x3 inertia;
    };

    Containers::StridedArrayView1D<Parent> parents;
    Containers::StridedArrayView1D<Transformation> transformations;

    Containers::StridedArrayView1D<JointAxis> jointAxes;
    Containers::StridedArrayView1D<JointLimit> jointLimits;
    Containers::StridedArrayView1D<JointDynamics> jointDynamics;

    Containers::StridedArrayView1D<LinkInertial> linkInertials;
    Containers::ArrayView<UnsignedInt> linkVisualMeshObjects;
    Containers::ArrayView<UnsignedInt> linkVisualMeshes;
    Containers::ArrayView<Int> linkVisualMeshMaterials;
    Containers::ArrayView<UnsignedInt> linkCollisionMeshObjects;
    Containers::ArrayView<UnsignedInt> linkCollisionMeshes;

    Containers::ArrayTuple data{
        {NoInit, parentCount, parents},
        {NoInit, transformationCount, transformations},

        {NoInit, jointAxesCount, jointAxes},
        {NoInit, jointLimitCount, jointLimits},
        {NoInit, jointDynamicsCount, jointDynamics},

        {NoInit, linkInertialCount, linkInertials},
        {NoInit, linkVisualCount, linkVisualMeshObjects},
        {NoInit, linkVisualCount, linkVisualMeshes},
        {NoInit, hasMaterials ? linkVisualCount : 0, linkVisualMeshMaterials},
        {NoInit, linkCollisionCount, linkCollisionMeshObjects},
        {NoInit, linkCollisionCount, linkCollisionMeshes},
    };

    // TODO fill the data
    std::size_t parentOffset = 0;
    std::size_t transformationOffset = 0;
    std::size_t jointAxesOffset = 0;
    std::size_t jointLimitOffset = 0;
    std::size_t jointDynamicsOffset = 0;
    std::size_t linkInertialOffset = 0;
    std::size_t linkVisualOffset = 0;
    std::size_t linkCollisionOffset = 0;
    for(const pugi::xml_node node: _in->doc.root().child("robot").children()) {
        const UnsignedInt id = parentOffset;

        if(node.name() == "joint"_s) {
            parents[parentOffset].mapping = id;
            parents[parentOffset].parent = -1; // TODO
            ++parentOffset;

            if(const pugi::xml_node origin = node.child("origin")) {
                transformations[transformationOffset].mapping = id;

                if(const pugi::xml_attribute rpy = origin.attribute("rpy")) {
                    // TODO hacky, doesn't have error handling
                    Vector3 data = Utility::ConfigurationValue<Vector3>::fromString(rpy.as_string(), {});
                    // TODO is this alright?!
                    transformations[transformationOffset].rotation =
                        Quaternion::rotation(Rad(data.z()), Vector3::yAxis())*
                        Quaternion::rotation(Rad(data.y()), Vector3::zAxis())*
                        Quaternion::rotation(Rad(data.x()), Vector3::xAxis());
                } else transformations[transformationOffset].rotation = {};

                if(const pugi::xml_attribute xyz = origin.attribute("xyz")) {
                    // TODO hacky, doesn't have error handling
                    transformations[transformationOffset].translation = Utility::ConfigurationValue<Vector3>::fromString(xyz.as_string(), {});
                    // is Y up or how??
                } else transformations[transformationOffset].translation = {};

                ++transformationOffset;
            }

            if(node.child("axis")) {
                jointAxes[jointAxesOffset].mapping = id;
                // TODO
                ++jointAxesOffset;
            }
            if(const pugi::xml_node dynamics = node.child("dynamics")) {
                jointDynamics[jointDynamicsOffset].mapping = id;
                if(const pugi::xml_attribute damping = dynamics.attribute("damping")) {
                    jointDynamics[jointDynamicsOffset].damping = damping.as_float();
                } // TODO else? default value? error? what if not float?
                // TODO friction?
                ++jointDynamicsOffset;
            }
            if(const pugi::xml_node limit = node.child("limit")) {
                jointLimits[jointLimitOffset].mapping = id;
                if(const pugi::xml_attribute effort = limit.attribute("effort")) {
                    jointLimits[jointLimitOffset].effort = effort.as_float();
                } // TODO else? default value? error? what if not float?
                if(const pugi::xml_attribute lower = limit.attribute("lower")) {
                    jointLimits[jointLimitOffset].lower = lower.as_float();
                } // TODO else? default value? error? what if not float?
                if(const pugi::xml_attribute upper = limit.attribute("upper")) {
                    jointLimits[jointLimitOffset].upper = upper.as_float();
                } // TODO else? default value? error? what if not float?
                if(const pugi::xml_attribute velocity = limit.attribute("velocity")) {
                    jointLimits[jointLimitOffset].velocity = velocity.as_float();
                } // TODO else? default value? error? what if not float?
                ++jointLimitOffset;
            }

        } else if(node.name() == "link"_s) {
            parents[parentOffset].mapping = id;
            parents[parentOffset].parent = -1; // TODO
            ++parentOffset;

            if(const pugi::xml_node inertial = node.child("inertial")) {
                const UnsignedInt inertialId = parentOffset;
                parents[parentOffset].mapping = inertialId;
                parents[parentOffset].parent = id;
                ++parentOffset;

                linkInertials[linkInertialOffset].mapping = inertialId;
                // TODO
                const pugi::xml_node mass = inertial.child("mass");
                pugi::xml_attribute massValue;
                if(mass && (massValue = mass.attribute("value"))) {
                    linkInertials[linkInertialOffset].mass = massValue.as_float();
                } // TODO else default? error if value not present?? what if not float?
                ++linkInertialOffset;

                if(const pugi::xml_node origin = inertial.child("origin")) {
                    transformations[transformationOffset].mapping = inertialId;
                    // TODO AHHHH DUPLICATES

                    if(const pugi::xml_attribute rpy = origin.attribute("rpy")) {
                        // TODO hacky, doesn't have error handling
                        Vector3 data = Utility::ConfigurationValue<Vector3>::fromString(rpy.as_string(), {});
                        // TODO is this alright?!
                        transformations[transformationOffset].rotation =
                            Quaternion::rotation(Rad(data.z()), Vector3::yAxis())*
                            Quaternion::rotation(Rad(data.y()), Vector3::zAxis())*
                            Quaternion::rotation(Rad(data.x()), Vector3::xAxis());
                    } else transformations[transformationOffset].rotation = {};

                    if(const pugi::xml_attribute xyz = origin.attribute("xyz")) {
                        // TODO hacky, doesn't have error handling
                        transformations[transformationOffset].translation = Utility::ConfigurationValue<Vector3>::fromString(xyz.as_string(), {});
                        // is Y up or how??
                    } else transformations[transformationOffset].translation = {};

                    ++transformationOffset;
                }
            }
            if(const pugi::xml_node visual = node.child("visual")) {
                const UnsignedInt visualId = parentOffset;
                parents[parentOffset].mapping = visualId;
                parents[parentOffset].parent = id;
                ++parentOffset;

                linkVisualMeshObjects[linkVisualOffset] = visualId;
                // TODO
                ++linkVisualOffset;

                if(const pugi::xml_node origin = visual.child("origin")) {
                    transformations[transformationOffset].mapping = visualId;

                    // TODO duplicates!!!
                    if(const pugi::xml_attribute rpy = origin.attribute("rpy")) {
                        // TODO hacky, doesn't have error handling
                        Vector3 data = Utility::ConfigurationValue<Vector3>::fromString(rpy.as_string(), {});
                        // TODO is this alright?!
                        transformations[transformationOffset].rotation =
                            Quaternion::rotation(Rad(data.z()), Vector3::yAxis())*
                            Quaternion::rotation(Rad(data.y()), Vector3::zAxis())*
                            Quaternion::rotation(Rad(data.x()), Vector3::xAxis());
                    } else transformations[transformationOffset].rotation = {};

                    if(const pugi::xml_attribute xyz = origin.attribute("xyz")) {
                        // TODO hacky, doesn't have error handling
                        transformations[transformationOffset].translation = Utility::ConfigurationValue<Vector3>::fromString(xyz.as_string(), {});
                        // is Y up or how??
                    } else transformations[transformationOffset].translation = {};

                    ++transformationOffset;
                }
                if(visual.child("material")) {
                    // TODO, without the incremented linkVisualOffset!!
                }
            }
            if(const pugi::xml_node collision = node.child("collision")) {
                const UnsignedInt collisionId = parentOffset;
                parents[parentOffset].mapping = collisionId;
                parents[parentOffset].parent = id;
                ++parentOffset;

                linkCollisionMeshObjects[linkCollisionOffset] = collisionId;
                // TODO
                ++linkCollisionOffset;

                if(const pugi::xml_node origin = collision.child("origin")) {
                    transformations[transformationOffset].mapping = collisionId;
                    // TODO duplicated code, 3 times!!!
                    if(const pugi::xml_attribute rpy = origin.attribute("rpy")) {
                        // TODO hacky, doesn't have error handling
                        Vector3 data = Utility::ConfigurationValue<Vector3>::fromString(rpy.as_string(), {});
                        // TODO is this alright?!
                        transformations[transformationOffset].rotation =
                            Quaternion::rotation(Rad(data.z()), Vector3::yAxis())*
                            Quaternion::rotation(Rad(data.y()), Vector3::zAxis())*
                            Quaternion::rotation(Rad(data.x()), Vector3::xAxis());
                    } else transformations[transformationOffset].rotation = {};

                    if(const pugi::xml_attribute xyz = origin.attribute("xyz")) {
                        // TODO hacky, doesn't have error handling
                        transformations[transformationOffset].translation = Utility::ConfigurationValue<Vector3>::fromString(xyz.as_string(), {});
                        // is Y up or how??
                    } else transformations[transformationOffset].translation = {};


                    ++transformationOffset;
                }
            }
        }
    }

    // TODO assert offsets match

    Containers::Array<SceneFieldData> fields;
    // TODO these are first now because GltfSceneConverter puts them to extra if some custom field is before, put them back before linkCollisionCount
    if(linkVisualCount) arrayAppend(fields, {
        SceneFieldData{SceneField::Mesh, linkVisualMeshObjects, linkVisualMeshes}
    });
    if(hasMaterials) arrayAppend(fields, {
        SceneFieldData{SceneField::MeshMaterial, linkVisualMeshObjects, linkVisualMeshMaterials}
    });
    arrayAppend(fields, {
        /* These are always present */
        SceneFieldData{SceneField::Parent, parents.slice(&Parent::mapping), parents.slice(&Parent::parent)},
        SceneFieldData{SceneField::Translation, transformations.slice(&Transformation::mapping), transformations.slice(&Transformation::translation)},
        SceneFieldData{SceneField::Rotation, transformations.slice(&Transformation::mapping), transformations.slice(&Transformation::rotation)},
    });
    if(jointAxesCount) arrayAppend(fields, {
        SceneFieldData{SceneFieldJointAxis, jointAxes.slice(&JointAxis::mapping), jointAxes.slice(&JointAxis::axis)}
    });
    if(jointLimitCount) arrayAppend(fields, {
        SceneFieldData{SceneFieldJointLimitLower, jointLimits.slice(&JointLimit::mapping), jointLimits.slice(&JointLimit::lower)},
        SceneFieldData{SceneFieldJointLimitUpper, jointLimits.slice(&JointLimit::mapping), jointLimits.slice(&JointLimit::upper)},
        SceneFieldData{SceneFieldJointLimitEffort, jointLimits.slice(&JointLimit::mapping), jointLimits.slice(&JointLimit::effort)},
        SceneFieldData{SceneFieldJointLimitVelocity, jointLimits.slice(&JointLimit::mapping), jointLimits.slice(&JointLimit::velocity)},
    });
    if(jointDynamicsCount) arrayAppend(fields, {
        SceneFieldData{SceneFieldJointDynamicsDamping, jointDynamics.slice(&JointDynamics::mapping), jointDynamics.slice(&JointDynamics::damping)},
        SceneFieldData{SceneFieldJointDynamicsFriction, jointDynamics.slice(&JointDynamics::mapping), jointDynamics.slice(&JointDynamics::friction)},
    });
    if(linkInertialCount) arrayAppend(fields, {
        SceneFieldData{SceneFieldLinkInertialMass, linkInertials.slice(&LinkInertial::mapping), linkInertials.slice(&LinkInertial::mass)},
        SceneFieldData{SceneFieldLinkInertia, linkInertials.slice(&LinkInertial::mapping), linkInertials.slice(&LinkInertial::inertia)},
    });
    if(linkCollisionCount) arrayAppend(fields, {
        SceneFieldData{SceneFieldCollisionMesh, linkCollisionMeshObjects, linkCollisionMeshes}
    });

    /* Convert back to the default deleter to avoid dangling deleter function
       pointer issues when unloading the plugin */
    arrayShrink(fields, DefaultInit);
    return SceneData{SceneMappingType::UnsignedInt, parentCount, std::move(data), std::move(fields)};
}

UnsignedLong UrdfImporter::doObjectCount() const {
    return _in->nodeNames.size();
}

Long UrdfImporter::doObjectForName(const Containers::StringView name) {
    const auto found = _in->nodesForName.find(name);
    return found == _in->nodesForName.end() ? -1 : found->second;
}

Containers::String UrdfImporter::doObjectName(const UnsignedLong id) {
    return _in->nodeNames[id];
}

}}

CORRADE_PLUGIN_REGISTER(UrdfImporter, Magnum::Trade::UrdfImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
