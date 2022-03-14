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

#include "PrimitiveImporter.h"

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
#include <Magnum/Trade/SceneData.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

PrimitiveImporter::PrimitiveImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

PrimitiveImporter::~PrimitiveImporter() = default;

ImporterFeatures PrimitiveImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool PrimitiveImporter::doIsOpened() const { return _opened; }

void PrimitiveImporter::doClose() { _opened = false; }

void PrimitiveImporter::doOpenData(Containers::Array<char>&&, DataFlags) {
    _opened = true;
}

namespace {

constexpr Containers::StringView Names[]{
    /*  0 */ "axis2D"_s,
    /*  1 */ "axis3D"_s,

    /*  2 */ "capsule2DWireframe"_s,
    /*  3 */ "capsule3DSolid"_s,
    /*  4 */ "capsule3DWireframe"_s,

    /*  5 */ "circle2DSolid"_s,
    /*  6 */ "circle2DWireframe"_s,
    /*  7 */ "circle3DSolid"_s,
    /*  8 */ "circle3DWireframe"_s,

    /*  9 */ "coneSolid"_s,
    /* 10 */ "coneWireframe"_s,

    /* 11 */ "crosshair2D"_s,
    /* 12 */ "crosshair3D"_s,

    /* 13 */ "cubeSolid"_s,
    /* 14 */ "cubeSolidStrip"_s,
    /* 15 */ "cubeWireframe"_s,

    /* 16 */ "cylinderSolid"_s,
    /* 17 */ "cylinderWireframe"_s,

    /* 18 */ "gradient2D"_s,
    /* 19 */ "gradient2DHorizontal"_s,
    /* 20 */ "gradient2DVertical"_s,
    /* 21 */ "gradient3D"_s,
    /* 22 */ "gradient3DHorizontal"_s,
    /* 23 */ "gradient3DVertical"_s,

    /* 24 */ "grid3DSolid"_s,
    /* 25 */ "grid3DWireframe"_s,

    /* 26 */ "icosphereSolid"_s,
    /* 27 */ "icosphereWireframe"_s,

    /* 28 */ "line2D"_s,
    /* 29 */ "line3D"_s,

    /* 30 */ "planeSolid"_s,
    /* 31 */ "planeWireframe"_s,

    /* 32 */ "squareSolid"_s,
    /* 33 */ "squareWireframe"_s,

    /* 34 */ "uvSphereSolid"_s,
    /* 35 */ "uvSphereWireframe"_s
};

constexpr Vector2 translation2DForIndex(UnsignedInt id) {
    /** @todo use constexpr vector multiplication here once C++14 is used */
    return {3.0f*(-1.5f + Float(id % 4)), 3.0f*(-1.0f + Float(id / 4))};
}

constexpr struct {
    Int parent[1]; /* same for all */
    struct {
        Vector2 translation;
        UnsignedInt meshAndObject;
    } fields[11];
} Scene2D[]{{{-1}, {
    {translation2DForIndex(0),   0}, /* axis2D */
    {translation2DForIndex(1),   2}, /* capsule2DWireframe */
    {translation2DForIndex(2),   5}, /* circle2DSolid */
    {translation2DForIndex(3),   6}, /* circle2DWireframe */
    {translation2DForIndex(4),  11}, /* crosshair2D */
    {translation2DForIndex(5),  18}, /* gradient2D */
    {translation2DForIndex(6),  19}, /* gradient2DHorizontal */
    {translation2DForIndex(7),  20}, /* gradient2DVertical */
    {translation2DForIndex(8),  28}, /* line2D */
    {translation2DForIndex(9),  32}, /* squareSolid */
    {translation2DForIndex(10), 33}, /* squareWireframe */
}}};

constexpr Vector3 translation3DForIndex(UnsignedInt id) {
    /** @todo use constexpr vector multiplication here once C++14 is used */
    return {3.0f*(-1.5f + Float(id % 5)), 3.0f*(-1.0f + Float(id / 5)), 0.0f};
}

constexpr struct {
    Int parent[1]; /* same for all */
    struct {
        Vector3 translation;
        UnsignedInt meshAndObject;
    } fields[25];
} Scene3D[]{{{-1}, {
    {translation3DForIndex(0),   1}, /* axis3D */
    {translation3DForIndex(1),   3}, /* capsule3DSolid */
    {translation3DForIndex(2),   4}, /* capsule3DWireframe */
    {translation3DForIndex(3),   7}, /* circle3DSolid */
    {translation3DForIndex(4),   8}, /* circle3DWireframe */
    {translation3DForIndex(5),   9}, /* coneSolid */
    {translation3DForIndex(6),  10}, /* coneWireframe */
    {translation3DForIndex(7),  12}, /* crosshair3D */
    {translation3DForIndex(8),  13}, /* cubeSolid */
    {translation3DForIndex(9),  14}, /* cubeSolidStrip */
    {translation3DForIndex(10), 15}, /* cubeWireframe */
    {translation3DForIndex(11), 16}, /* cylinderSolid */
    {translation3DForIndex(12), 17}, /* cylinderWireframe */
    {translation3DForIndex(13), 21}, /* gradient3D */
    {translation3DForIndex(14), 22}, /* gradient3DHorizontal */
    {translation3DForIndex(15), 23}, /* gradient3DVertical */
    {translation3DForIndex(16), 24}, /* grid3DSolid */
    {translation3DForIndex(17), 25}, /* grid3DWireframe */
    {translation3DForIndex(18), 26}, /* icosphereSolid */
    {translation3DForIndex(19), 27}, /* icosphereWireframe */
    {translation3DForIndex(20), 29}, /* line3D */
    {translation3DForIndex(21), 30}, /* planeSolid */
    {translation3DForIndex(22), 31}, /* planeWireframe */
    {translation3DForIndex(23), 34}, /* uvSphereSolid */
    {translation3DForIndex(24), 35}, /* uvSphereWireframe */
}}};

/* StridedArrayView slice() and broadcast() is not constexpr so I have to
   supply the strides by hand */
#ifndef CORRADE_MSVC2015_COMPATIBILITY
/* MSVC 2015 ICEs if both Scene3D and this are marked as constexpr. At first I
   thought it's due to the float operations and nested function calls in the
   Scene2D initializer, but that alone works... so I guess it's the combination
   of that and the fields being referenced here. It however has to be noted
   that even the SceneFieldData construction test itself doesn't work with
   constexpr on MSVC 2015. */
constexpr
#else
const
#endif
SceneFieldData SceneFields2D[]{
    SceneFieldData{SceneField::Parent,
        Containers::stridedArrayView(Scene2D, &Scene2D->fields[0].meshAndObject, Containers::arraySize(Scene2D->fields), sizeof(Scene2D->fields[0])),
        Containers::stridedArrayView(Containers::arrayView(Scene2D->parent), Containers::arraySize(Scene2D->fields), 0)},
    SceneFieldData{SceneField::Mesh,
        Containers::stridedArrayView(Scene2D, &Scene2D->fields[0].meshAndObject, Containers::arraySize(Scene2D->fields), sizeof(Scene2D->fields[0])), Containers::stridedArrayView(Scene2D, &Scene2D->fields[0].meshAndObject, Containers::arraySize(Scene2D->fields), sizeof(Scene2D->fields[0]))},
    SceneFieldData{SceneField::Translation,
        Containers::stridedArrayView(Scene2D, &Scene2D->fields[0].meshAndObject, Containers::arraySize(Scene2D->fields), sizeof(Scene2D->fields[0])), Containers::stridedArrayView(Scene2D, &Scene2D->fields[0].translation, Containers::arraySize(Scene2D->fields), sizeof(Scene2D->fields[0]))},
};

/* StridedArrayView slice() and broadcast() is not constexpr so I have to
   supply the strides by hand */
#ifndef CORRADE_MSVC2015_COMPATIBILITY
/* MSVC 2015 ICEs if both Scene3D and this are marked as constexpr. At first I
   thought it's due to the float operations and nested function calls in the
   Scene2D initializer, but that alone works... so I guess it's the combination
   of that and the fields being referenced here. It however has to be noted
   that even the SceneFieldData construction test itself doesn't work with
   constexpr on MSVC 2015. */
constexpr
#else
const
#endif
SceneFieldData SceneFields3D[]{
    SceneFieldData{SceneField::Parent,
        Containers::stridedArrayView(Scene3D, &Scene3D->fields[0].meshAndObject, Containers::arraySize(Scene3D->fields), sizeof(Scene3D->fields[0])),
        Containers::stridedArrayView(Containers::arrayView(Scene3D->parent), Containers::arraySize(Scene3D->fields), 0)},
    SceneFieldData{SceneField::Mesh,
        Containers::stridedArrayView(Scene3D, &Scene3D->fields[0].meshAndObject, Containers::arraySize(Scene3D->fields), sizeof(Scene3D->fields[0])),
        Containers::stridedArrayView(Scene3D, &Scene3D->fields[0].meshAndObject, Containers::arraySize(Scene3D->fields), sizeof(Scene3D->fields[0]))},
    SceneFieldData{SceneField::Translation,
        Containers::stridedArrayView(Scene3D, &Scene3D->fields[0].meshAndObject, Containers::arraySize(Scene3D->fields), sizeof(Scene3D->fields[0])),
        Containers::stridedArrayView(Scene3D, &Scene3D->fields[0].translation, Containers::arraySize(Scene3D->fields), sizeof(Scene3D->fields[0]))},
};

}

Int PrimitiveImporter::doDefaultScene() const { return 1; }

UnsignedInt PrimitiveImporter::doSceneCount() const { return 2; }

Containers::Optional<SceneData> PrimitiveImporter::doScene(const UnsignedInt id) {
    if(id == 0) return SceneData{SceneMappingType::UnsignedInt,
        Containers::arraySize(Names),
        {}, Scene2D, sceneFieldDataNonOwningArray(SceneFields2D)};

    if(id == 1) return SceneData{SceneMappingType::UnsignedInt,
        Containers::arraySize(Names),
        {}, Scene3D, sceneFieldDataNonOwningArray(SceneFields3D)};

    CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

UnsignedLong PrimitiveImporter::doObjectCount() const {
    return Containers::arraySize(Names);
}

Long PrimitiveImporter::doObjectForName(const Containers::StringView name) {
    /** @todo it's sorted, a reusable binary search utility would be handy
        (not that hard-to-use-correctly STL lower_bound crap) */
    for(std::size_t i = 0; i != Containers::arraySize(Names); ++i)
        if(name == Names[i]) return i;
    return -1;
}

Containers::String PrimitiveImporter::doObjectName(const UnsignedLong id) {
    return Names[id];
}

UnsignedInt PrimitiveImporter::doMeshCount() const {
    return Containers::arraySize(Names);
}

Int PrimitiveImporter::doMeshForName(const Containers::StringView name) {
    /** @todo it's sorted, a reusable binary search utility would be handy
        (not that hard-to-use-correctly STL lower_bound crap) */
    for(std::size_t i = 0; i != Containers::arraySize(Names); ++i)
        if(name == Names[i]) return i;
    return -1;
}

Containers::String PrimitiveImporter::doMeshName(const UnsignedInt id) {
    return Names[id];
}

Containers::Optional<MeshData> PrimitiveImporter::doMesh(UnsignedInt id, UnsignedInt) {
    if(Names[id] == "axis2D"_s)
        return Primitives::axis2D();

    if(Names[id] == "axis3D"_s)
        return Primitives::axis3D();

    if(Names[id] == "capsule2DWireframe"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("capsule2DWireframe"));

        return Primitives::capsule2DWireframe(
            conf->value<UnsignedInt>("hemisphereRings"),
            conf->value<UnsignedInt>("cylinderRings"),
            conf->value<Float>("halfLength"));
    }

    if(Names[id] == "capsule3DSolid"_s) {
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

    if(Names[id] == "capsule3DWireframe"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("capsule3DWireframe"));

        return Primitives::capsule3DWireframe(
            conf->value<UnsignedInt>("hemisphereRings"),
            conf->value<UnsignedInt>("cylinderRings"),
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"));
    }

    if(Names[id] == "circle2DSolid"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("circle2DSolid"));

        Primitives::Circle2DFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::Circle2DFlag::TextureCoordinates;

        return Primitives::circle2DSolid(
            conf->value<UnsignedInt>("segments"),
            flags);
    }

    if(Names[id] == "circle2DWireframe"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("circle2DWireframe"));

        return Primitives::circle2DWireframe(
            conf->value<UnsignedInt>("segments"));
    }

    if(Names[id] == "circle3DSolid"_s) {
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

    if(Names[id] == "circle3DWireframe"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("circle3DWireframe"));

        return Primitives::circle3DWireframe(
            conf->value<UnsignedInt>("segments"));
    }

    if(Names[id] == "coneSolid"_s) {
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

    if(Names[id] == "coneWireframe"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("coneWireframe"));

        return Primitives::coneWireframe(
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"));
    }

    if(Names[id] == "crosshair2D"_s)
        return Primitives::crosshair2D();

    if(Names[id] == "crosshair3D"_s)
        return Primitives::crosshair3D();

    if(Names[id] == "cubeSolid"_s)
        return Primitives::cubeSolid();

    if(Names[id] == "cubeSolidStrip"_s)
        return Primitives::cubeSolidStrip();

    if(Names[id] == "cubeWireframe"_s)
        return Primitives::cubeWireframe();

    if(Names[id] == "cylinderSolid"_s) {
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

    if(Names[id] == "cylinderWireframe"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("cylinderWireframe"));

        return Primitives::cylinderWireframe(
            conf->value<UnsignedInt>("rings"),
            conf->value<UnsignedInt>("segments"),
            conf->value<Float>("halfLength"));
    }

    if(Names[id] == "gradient2D"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient2D"));

        return Primitives::gradient2D(
            conf->value<Vector2>("a"),
            conf->value<Color4>("colorA"),
            conf->value<Vector2>("b"),
            conf->value<Color4>("colorB"));
    }

    if(Names[id] == "gradient2DHorizontal"_s) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 2D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient2D"));

        return Primitives::gradient2DHorizontal(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(Names[id] == "gradient2DVertical"_s) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 2D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient2D"));

        return Primitives::gradient2DVertical(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(Names[id] == "gradient3D"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient3D"));

        return Primitives::gradient3D(
            conf->value<Vector3>("a"),
            conf->value<Color4>("colorA"),
            conf->value<Vector3>("b"),
            conf->value<Color4>("colorB"));
    }

    if(Names[id] == "gradient3DHorizontal"_s) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 3D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient3D"));

        return Primitives::gradient3DHorizontal(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(Names[id] == "gradient3DVertical"_s) {
        Utility::ConfigurationGroup* conf;
        /* The same config shared for all 3D gradients */
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("gradient3D"));

        return Primitives::gradient3DVertical(
            conf->value<Color4>("colorA"),
            conf->value<Color4>("colorB"));
    }

    if(Names[id] == "grid3DSolid"_s) {
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

    if(Names[id] == "grid3DWireframe"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("grid3DWireframe"));

        return Primitives::grid3DWireframe(
            conf->value<Vector2i>("subdivisions"));
    }

    if(Names[id] == "icosphereSolid"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("icosphereSolid"));

        return Primitives::icosphereSolid(
            conf->value<UnsignedInt>("subdivisions"));
    }

    if(Names[id] == "icosphereWireframe"_s)
        return Primitives::icosphereWireframe();

    if(Names[id] == "line2D"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("line2D"));

        return Primitives::line2D(
            conf->value<Vector2>("a"),
            conf->value<Vector2>("b"));
    }

    if(Names[id] == "line3D"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("line3D"));

        return Primitives::line3D(
            conf->value<Vector3>("a"),
            conf->value<Vector3>("b"));
    }

    if(Names[id] == "planeSolid"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("planeSolid"));

        Primitives::PlaneFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::PlaneFlag::TextureCoordinates;
        if(conf->value<bool>("tangents"))
            flags |= Primitives::PlaneFlag::Tangents;

        return Primitives::planeSolid(flags);
    }

    if(Names[id] == "planeWireframe"_s)
        return Primitives::planeWireframe();

    if(Names[id] == "squareSolid"_s) {
        Utility::ConfigurationGroup* conf;
        CORRADE_INTERNAL_ASSERT_OUTPUT(conf = configuration().group("squareSolid"));

        Primitives::SquareFlags flags;
        if(conf->value<bool>("textureCoordinates"))
            flags |= Primitives::SquareFlag::TextureCoordinates;

        return Primitives::squareSolid(flags);
    }

    if(Names[id] == "squareWireframe"_s)
        return Primitives::squareWireframe();

    if(Names[id] == "uvSphereSolid"_s) {
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

    if(Names[id] == "uvSphereWireframe"_s) {
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
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
