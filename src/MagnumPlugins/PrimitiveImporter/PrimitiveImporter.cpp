/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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

#include "PrimitiveImporter.h"

#include <cstring>
#include <numeric>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/Math/ConfigurationValue.h>
#include <Magnum/Primitives/Axis.h>
#include <Magnum/Primitives/Capsule.h>
#include <Magnum/Primitives/Circle.h>
#include <Magnum/Primitives/Cone.h>
#include <Magnum/Primitives/Crosshair.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Cylinder.h>
#include <Magnum/Primitives/Gradient.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Primitives/Line.h>
#include <Magnum/Primitives/Plane.h>
#include <Magnum/Primitives/Square.h>
#include <Magnum/Primitives/UVSphere.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData2D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>

namespace Magnum { namespace Trade {

PrimitiveImporter::PrimitiveImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

PrimitiveImporter::~PrimitiveImporter() = default;

ImporterFeatures PrimitiveImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool PrimitiveImporter::doIsOpened() const { return _opened; }

void PrimitiveImporter::doClose() { _opened = false; }

void PrimitiveImporter::doOpenData(Containers::ArrayView<const char>) {
    _opened = true;
}

namespace {

constexpr const char* Names[]{
    "axis2D",
    "axis3D",

    "capsule2DWireframe",
    "capsule3DSolid",
    "capsule3DWireframe",

    "circle2DSolid",
    "circle2DWireframe",
    "circle3DSolid",
    "circle3DWireframe",

    "coneSolid",
    "coneWireframe",

    "crosshair2D",
    "crosshair3D",

    "cubeSolid",
    "cubeSolidStrip",
    "cubeWireframe",

    "cylinderSolid",
    "cylinderWireframe",

    "gradient2D",
    "gradient2DHorizontal",
    "gradient2DVertical",
    "gradient3D",
    "gradient3DHorizontal",
    "gradient3DVertical",

    "grid3DSolid",
    "grid3DWireframe",

    "icosphereSolid",
    "icosphereWireframe",

    "line2D",
    "line3D",

    "planeSolid",
    "planeWireframe",

    "squareSolid",
    "squareWireframe",

    "uvSphereSolid",
    "uvSphereWireframe"
};

constexpr const char* Names2D[]{
    "axis2D",
    "capsule2DWireframe",
    "circle2DSolid",
    "circle2DWireframe",
    "crosshair2D",
    "gradient2D",
    "gradient2DHorizontal",
    "gradient2DVertical",
    "line2D",
    "squareSolid",
    "squareWireframe"
};

constexpr const char* Names3D[]{
    "axis3D",
    "capsule3DSolid",
    "capsule3DWireframe",
    "circle3DSolid",
    "circle3DWireframe",
    "coneSolid",
    "coneWireframe",
    "crosshair3D",
    "cubeSolid",
    "cubeSolidStrip",
    "cubeWireframe",
    "cylinderSolid",
    "cylinderWireframe",
    "gradient3D",
    "gradient3DHorizontal",
    "gradient3DVertical",
    "grid3DSolid",
    "grid3DWireframe",
    "icosphereSolid",
    "icosphereWireframe",
    "line3D",
    "planeSolid",
    "planeWireframe",
    "uvSphereSolid",
    "uvSphereWireframe"
};

}

Int PrimitiveImporter::doDefaultScene() const { return 0; }

UnsignedInt PrimitiveImporter::doSceneCount() const { return 1; }

Containers::Optional<SceneData> PrimitiveImporter::doScene(UnsignedInt) {
    std::vector<UnsignedInt> children2D(Containers::arraySize(Names2D));
    std::iota(children2D.begin(), children2D.end(), 0);
    std::vector<UnsignedInt> children3D(Containers::arraySize(Names3D));
    std::iota(children3D.begin(), children3D.end(), 0);

    return SceneData{std::move(children2D), std::move(children3D)};
}

UnsignedInt PrimitiveImporter::doObject2DCount() const {
    return Containers::arraySize(Names2D);
}

Int PrimitiveImporter::doObject2DForName(const std::string& name) {
    for(std::size_t i = 0; i != Containers::arraySize(Names2D); ++i)
        if(name == Names2D[i]) return i;
    return -1;
}

std::string PrimitiveImporter::doObject2DName(const UnsignedInt id) {
    return Names2D[id];
}

Containers::Pointer<ObjectData2D> PrimitiveImporter::doObject2D(const UnsignedInt id) {
    static_assert(Containers::arraySize(Names2D) <= 12, "can't pack into 3x4 cells");
    return Containers::pointer(new MeshObjectData2D{{},
        Matrix3::translation(3.0f*Vector2{-1.5f + Float(id % 4), -1.0f + Float(id / 4)}),
        UnsignedInt(doMeshForName(Names2D[id])), -1, -1
    });
}

UnsignedInt PrimitiveImporter::doObject3DCount() const {
    return Containers::arraySize(Names3D);
}

Int PrimitiveImporter::doObject3DForName(const std::string& name) {
    for(std::size_t i = 0; i != Containers::arraySize(Names3D); ++i)
        if(name == Names3D[i]) return i;
    return -1;
}

std::string PrimitiveImporter::doObject3DName(const UnsignedInt id) {
    return Names3D[id];
}

Containers::Pointer<ObjectData3D> PrimitiveImporter::doObject3D(const UnsignedInt id) {
    static_assert(Containers::arraySize(Names3D) <= 25, "can't pack into 5x5 cells");
    return Containers::pointer(new MeshObjectData3D{{},
        Matrix4::translation(3.0f*Vector3{-2.0f + Float(id % 5), -2.0f + Float(id / 5), 0.0f}),
        UnsignedInt(doMeshForName(Names3D[id])), -1, -1
    });
}

UnsignedInt PrimitiveImporter::doMeshCount() const {
    return Containers::arraySize(Names);
}

Int PrimitiveImporter::doMeshForName(const std::string& name) {
    for(std::size_t i = 0; i != Containers::arraySize(Names); ++i)
        if(name == Names[i]) return i;
    return -1;
}

std::string PrimitiveImporter::doMeshName(const UnsignedInt id) {
    return Names[id];
}

Containers::Optional<MeshData> PrimitiveImporter::doMesh(UnsignedInt id, UnsignedInt) {
    if(std::strcmp(Names[id], "axis2D") == 0)
        return Primitives::axis2D();

    if(std::strcmp(Names[id], "axis3D") == 0)
        return Primitives::axis3D();

    if(std::strcmp(Names[id], "capsule2DWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("capsule2DWireframe"));

        return Primitives::capsule2DWireframe(
            conf->value<UnsignedInt>("hemisphereRings"),
            conf->value<UnsignedInt>("cylinderRings"),
            conf->value<Float>("halfLength"));
    }

    if(std::strcmp(Names[id], "capsule3DSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("capsule3DSolid"));

        Primitives::CapsuleFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::CapsuleFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::CapsuleFlag::Tangents;

        return Primitives::capsule3DSolid(
            conf->value<UnsignedInt>("hemisphereRings"),
            conf->value<UnsignedInt>("cylinderRings"),
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"),
            flags);
    }

    if(std::strcmp(Names[id], "capsule3DWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("capsule3DWireframe"));

        return Primitives::capsule3DWireframe(
            conf->value<UnsignedInt>("hemisphereRings"),
            conf->value<UnsignedInt>("cylinderRings"),
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"));
    }

    if(std::strcmp(Names[id], "circle2DSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("circle2DSolid"));

        Primitives::Circle2DFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::Circle2DFlag::TextureCoordinates;

        return Primitives::circle2DSolid(
            conf->value<UnsignedInt>("segments"),
            flags);
    }

    if(std::strcmp(Names[id], "circle2DWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("circle2DWireframe"));

        return Primitives::circle2DWireframe(
            conf->value<UnsignedInt>("segments"));
    }

    if(std::strcmp(Names[id], "circle3DSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("circle3DSolid"));

        Primitives::Circle3DFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::Circle3DFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::Circle3DFlag::Tangents;

        return Primitives::circle3DSolid(
            conf->value<UnsignedInt>("segments"),
            flags);
    }

    if(std::strcmp(Names[id], "circle3DWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("circle3DWireframe"));

        return Primitives::circle3DWireframe(
            conf->value<UnsignedInt>("segments"));
    }

    if(std::strcmp(Names[id], "coneSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("coneSolid"));

        Primitives::ConeFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::ConeFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::ConeFlag::Tangents;
        if(conf->value<bool>("capEnd"))
            flags |= Primitives::ConeFlag::CapEnd;

        return Primitives::coneSolid(
            conf->value<UnsignedInt>("rings"),
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"),
            flags);
    }

    if(std::strcmp(Names[id], "coneWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("coneWireframe"));

        return Primitives::coneWireframe(
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"));
    }

    if(std::strcmp(Names[id], "crosshair2D") == 0)
        return Primitives::crosshair2D();

    if(std::strcmp(Names[id], "crosshair3D") == 0)
        return Primitives::crosshair3D();

    if(std::strcmp(Names[id], "cubeSolid") == 0)
        return Primitives::cubeSolid();

    if(std::strcmp(Names[id], "cubeSolidStrip") == 0)
        return Primitives::cubeSolidStrip();

    if(std::strcmp(Names[id], "cubeWireframe") == 0)
        return Primitives::cubeWireframe();

    if(std::strcmp(Names[id], "cylinderSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("cylinderSolid"));

        Primitives::CylinderFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::CylinderFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::CylinderFlag::Tangents;
        if(conf->value<bool>("capEnds"))
            flags |= Primitives::CylinderFlag::CapEnds;

        return Primitives::cylinderSolid(
            conf->value<UnsignedInt>("rings"),
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"),
            flags);
    }

    if(std::strcmp(Names[id], "cylinderWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("cylinderWireframe"));

        return Primitives::cylinderWireframe(
            conf->value<UnsignedInt>("rings"),
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"));
    }

    if(std::strcmp(Names[id], "gradient2D") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient2D"));

        return Primitives::gradient2D(
            conf->value<Vector2>("a"),
            conf->value<Color4>("colorA"),
            conf->value<Vector2>("b"),
            conf->value<Color4>("colorB"));
    }

    if(std::strcmp(Names[id], "gradient2DHorizontal") == 0) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 2D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient2D"));

        return Primitives::gradient2DHorizontal(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(std::strcmp(Names[id], "gradient2DVertical") == 0) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 2D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient2D"));

        return Primitives::gradient2DVertical(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(std::strcmp(Names[id], "gradient3D") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient3D"));

        return Primitives::gradient3D(
            conf->value<Vector3>("a"),
            conf->value<Color4>("colorA"),
            conf->value<Vector3>("b"),
            conf->value<Color4>("colorB"));
    }

    if(std::strcmp(Names[id], "gradient3DHorizontal") == 0) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 3D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient3D"));

        return Primitives::gradient3DHorizontal(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(std::strcmp(Names[id], "gradient3DVertical") == 0) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 3D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient3D"));

        return Primitives::gradient3DVertical(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(std::strcmp(Names[id], "grid3DSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("grid3DSolid"));

        Primitives::GridFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::GridFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::GridFlag::Tangents;
        if(conf->value<bool>("normals"))
            flags |= Primitives::GridFlag::Normals;

        return Primitives::grid3DSolid(
            conf->value<Vector2i>("subdivisions"),
            flags);
    }

    if(std::strcmp(Names[id], "grid3DWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("grid3DWireframe"));

        return Primitives::grid3DWireframe(
            conf->value<Vector2i>("subdivisions"));
    }

    if(std::strcmp(Names[id], "icosphereSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("icosphereSolid"));

        return Primitives::icosphereSolid(
            conf->value<UnsignedInt>("subdivisions"));
    }

    if(std::strcmp(Names[id], "icosphereWireframe") == 0)
        return Primitives::icosphereWireframe();

    if(std::strcmp(Names[id], "line2D") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("line2D"));

        return Primitives::line2D(
            conf->value<Vector2>("a"),
            conf->value<Vector2>("b"));
    }

    if(std::strcmp(Names[id], "line3D") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("line3D"));

        return Primitives::line3D(
            conf->value<Vector3>("a"),
            conf->value<Vector3>("b"));
    }

    if(std::strcmp(Names[id], "planeSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("planeSolid"));

        Primitives::PlaneFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::PlaneFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::PlaneFlag::Tangents;

        return Primitives::planeSolid(flags);
    }

    if(std::strcmp(Names[id], "planeWireframe") == 0)
        return Primitives::planeWireframe();

    if(std::strcmp(Names[id], "squareSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("squareSolid"));

        Primitives::SquareFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::SquareFlag::TextureCoordinates;

        return Primitives::squareSolid(flags);
    }

    if(std::strcmp(Names[id], "squareWireframe") == 0)
        return Primitives::squareWireframe();

    if(std::strcmp(Names[id], "uvSphereSolid") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("uvSphereSolid"));

        Primitives::UVSphereFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::UVSphereFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::UVSphereFlag::Tangents;

        return Primitives::uvSphereSolid(
            conf->value<UnsignedInt>("rings"),
            conf->value<UnsignedInt>("segments"),
            flags);
    }

    if(std::strcmp(Names[id], "uvSphereWireframe") == 0) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("uvSphereWireframe"));

        return Primitives::uvSphereWireframe(
            conf->value<UnsignedInt>("rings"),
            conf->value<UnsignedInt>("segments"));
    }

    CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

}}

CORRADE_PLUGIN_REGISTER(PrimitiveImporter, Magnum::Trade::PrimitiveImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
