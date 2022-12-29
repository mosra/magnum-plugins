#ifndef Magnum_Trade_UfbxMaterials_h
#define Magnum_Trade_UfbxMaterials_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Samuli Raivio <bqqbarbhg@gmail.com>

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

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringView.h>
#include <Magnum/Types.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MaterialLayerData.h>

#include "ufbx.h"

namespace Magnum { namespace Trade { namespace {

using namespace Containers::Literals;

enum class UfbxMaterialLayer: UnsignedInt {
    Base,
    Coat,
    Transmission,
    Subsurface,
    Sheen,
    Matte,
};

constexpr UnsignedInt UfbxMaterialLayerCount = UnsignedInt(UfbxMaterialLayer::Matte) + 1;

constexpr Containers::StringView ufbxMaterialLayerNames[]{
    {},
    "ClearCoat"_s,
    "transmission"_s,
    "subsurface"_s,
    "sheen"_s,
    "matte"_s,
};
static_assert(Containers::arraySize(ufbxMaterialLayerNames) == UfbxMaterialLayerCount, "Wrong amount of material layer names");

/* Some properties are represented as multiple alternatives and we want to
   pick only a single one. */
enum class MaterialExclusionGroup: UnsignedInt {
    NormalTexture = 1 << 0,
    Emission = 1 << 1,
    EmissionFactor = 1 << 2,
    Displacement = 1 << 3,
    DisplacementFactor = 1 << 4,
    SpecularColor = 1 << 5,
    SpecularFactor = 1 << 6,
};

typedef Containers::EnumSet<MaterialExclusionGroup> MaterialExclusionGroups;

struct MaterialMapping {
    /* Sentinel value to use as textureAttribute do disallow any texture for
       this mapping as empty is implicitly derived from attribute, see below */
    /* Note: This has to be a function instead of a value as otherwise it leads
       to linker errors in the array constructors below.
       "relocation R_X86_64_PC32 against undefined symbol
       `_ZN6Magnum5Trade12_GLOBAL__N_115MaterialMapping15DisallowTextureE' can
       not be used when making a shared object; recompile with -fPIC" */
    static constexpr Containers::StringView DisallowTexture() { return " "_s; };

    UfbxMaterialLayer layer;
    MaterialAttributeType attributeType;

    /* Named MaterialAttribute or a custom name */
    Containers::StringView attribute;

    /* Override the attribute of the texture, defaults to attribute+"Texture" */
    Containers::StringView textureAttribute;

    /* ufbx_material_map (or -1 for none) for the value */
    Int valueMap;

    /* ufbx_material_map (or -1 for none) for the factor. This is by default
       multiplied into the value of valueMap unless user explicitly asks for the
       factors */
    Int factorMap;

    /* Multiple MaterialMapping entries may have the same attribute name which
       is forbidden by MaterialData. UfbxImporter::doMaterial() keeps track of
       an EnumSet of these bits to prevent name collisions. These are validated
       to be exclusive at test time in UfbxImporterTest::materialMapping() */
    MaterialExclusionGroup exclusionGroup;

    constexpr MaterialMapping(UfbxMaterialLayer layer, MaterialAttributeType attributeType, Containers::StringView attribute, Containers::StringView textureAttribute, Int valueMap, Int factorMap=-1, MaterialExclusionGroup exclusionGroup=MaterialExclusionGroup{})
        : layer(layer), attributeType(attributeType), attribute(attribute), textureAttribute(textureAttribute), valueMap(valueMap), factorMap(factorMap), exclusionGroup(exclusionGroup) {}
};

constexpr MaterialMapping materialMappingFbx[]{
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "DiffuseColor"_s, "DiffuseTexture"_s, UFBX_MATERIAL_FBX_DIFFUSE_COLOR, UFBX_MATERIAL_FBX_DIFFUSE_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "SpecularColor"_s, "SpecularTexture"_s, UFBX_MATERIAL_FBX_SPECULAR_COLOR, UFBX_MATERIAL_FBX_SPECULAR_FACTOR, MaterialExclusionGroup::SpecularColor},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Shininess"_s, "shininessTexture"_s, UFBX_MATERIAL_FBX_SPECULAR_EXPONENT},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "reflectionColor"_s, "reflectionTexture"_s, UFBX_MATERIAL_FBX_REFLECTION_COLOR, UFBX_MATERIAL_FBX_REFLECTION_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "transparencyColor"_s, "transparencyTexture"_s, UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR, UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "EmissiveColor"_s, "EmissiveTexture"_s, UFBX_MATERIAL_FBX_EMISSION_COLOR, UFBX_MATERIAL_FBX_EMISSION_FACTOR, MaterialExclusionGroup::Emission},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "AmbientColor"_s, "AmbientTexture"_s, UFBX_MATERIAL_FBX_AMBIENT_COLOR, UFBX_MATERIAL_FBX_AMBIENT_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_FBX_NORMAL_MAP, -1, MaterialExclusionGroup::NormalTexture},
    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_FBX_BUMP, -1, MaterialExclusionGroup::NormalTexture},
    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "bumpTexture"_s, UFBX_MATERIAL_FBX_BUMP},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "bumpFactor"_s, {}, UFBX_MATERIAL_FBX_BUMP_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "displacementTexture"_s, UFBX_MATERIAL_FBX_DISPLACEMENT, -1, MaterialExclusionGroup::Displacement},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "displacementFactor"_s, {}, UFBX_MATERIAL_FBX_DISPLACEMENT_FACTOR, -1, MaterialExclusionGroup::DisplacementFactor},
    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "vectorDisplacementTexture"_s, UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "vectorDisplacementFactor"_s, {}, UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT_FACTOR},
};

constexpr MaterialMapping materialMappingFbxFactor[]{
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "diffuseColorFactor"_s, {}, UFBX_MATERIAL_FBX_DIFFUSE_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularColorFactor"_s, {}, UFBX_MATERIAL_FBX_SPECULAR_FACTOR, -1, MaterialExclusionGroup::SpecularFactor},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "reflectionColorFactor"_s, {}, UFBX_MATERIAL_FBX_REFLECTION_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "transparencyColorFactor"_s, {}, UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "emissiveColorFactor"_s, {}, UFBX_MATERIAL_FBX_EMISSION_FACTOR, -1, MaterialExclusionGroup::EmissionFactor},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "ambientColorFactor"_s, {}, UFBX_MATERIAL_FBX_AMBIENT_FACTOR},
};

constexpr MaterialMapping materialMappingPbr[]{
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "BaseColor"_s, {}, UFBX_MATERIAL_PBR_BASE_COLOR, UFBX_MATERIAL_PBR_BASE_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_ROUGHNESS},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Glossiness"_s, {}, UFBX_MATERIAL_PBR_GLOSSINESS},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Metalness"_s, {}, UFBX_MATERIAL_PBR_METALNESS},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "diffuseRoughness"_s, {}, UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS},

    /* Specular "layer", it's not really a layer as it modifies the specular
       implicitly defined by BaseColor and Metalness */
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "SpecularColor"_s, "SpecularTexture"_s, UFBX_MATERIAL_PBR_SPECULAR_COLOR, UFBX_MATERIAL_PBR_SPECULAR_FACTOR, MaterialExclusionGroup::SpecularColor},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularIor"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_IOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularAnisotropy"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularRotation"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_ROTATION},

    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Vector4, "color"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_COLOR},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "depth"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_DEPTH},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Vector3, "scatter"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "scatterAnisotropy"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER_ANISOTROPY},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "dispersion"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_DISPERSION},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_ROUGHNESS},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "Glossiness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_GLOSSINESS},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "extraRoughness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_EXTRA_ROUGHNESS},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Long, "priority"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_PRIORITY},
    {UfbxMaterialLayer::Transmission, MaterialAttributeType::Bool, "enableInAov"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_ENABLE_IN_AOV},

    {UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR},
    {UfbxMaterialLayer::Subsurface, MaterialAttributeType::Vector4, "color"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_COLOR},
    {UfbxMaterialLayer::Subsurface, MaterialAttributeType::Vector3, "radius"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_RADIUS},
    {UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "scale"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_SCALE},
    {UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "anisotropy"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY},
    {UfbxMaterialLayer::Subsurface, MaterialAttributeType::Vector4, "tintColor"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_TINT_COLOR},
    {UfbxMaterialLayer::Subsurface, MaterialAttributeType::Long, "type"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_TYPE},

    {UfbxMaterialLayer::Sheen, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_SHEEN_FACTOR},
    {UfbxMaterialLayer::Sheen, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_SHEEN_COLOR},
    {UfbxMaterialLayer::Sheen, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS},

    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_COAT_FACTOR},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Vector4, "color"_s, {}, UFBX_MATERIAL_PBR_COAT_COLOR},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_COAT_ROUGHNESS},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "Glossiness"_s, {}, UFBX_MATERIAL_PBR_COAT_GLOSSINESS},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "ior"_s, {}, UFBX_MATERIAL_PBR_COAT_IOR},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "anisotropy"_s, {}, UFBX_MATERIAL_PBR_COAT_ANISOTROPY},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "rotation"_s, {}, UFBX_MATERIAL_PBR_COAT_ROTATION},
    {UfbxMaterialLayer::Coat, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_PBR_COAT_NORMAL},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "affectBaseColor"_s, {}, UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_COLOR},
    {UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "affectBaseRoughness"_s, {}, UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_ROUGHNESS},

    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "thinFilmThickness"_s, {}, UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "thinFilmIor"_s, {}, UFBX_MATERIAL_PBR_THIN_FILM_IOR},

    /** @todo This could be its own layer */
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "EmissiveColor"_s, "EmissiveTexture"_s, UFBX_MATERIAL_PBR_EMISSION_COLOR, UFBX_MATERIAL_PBR_EMISSION_FACTOR, MaterialExclusionGroup::Emission},

    /* Patched to BaseColor.a if scalar */
    {UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "opacity"_s, {}, UFBX_MATERIAL_PBR_OPACITY},

    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "indirectDiffuse"_s, {}, UFBX_MATERIAL_PBR_INDIRECT_DIFFUSE},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "indirectSpecular"_s, {}, UFBX_MATERIAL_PBR_INDIRECT_SPECULAR},

    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_PBR_NORMAL_MAP, -1, MaterialExclusionGroup::NormalTexture},
    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "tangentTexture"_s, UFBX_MATERIAL_PBR_TANGENT_MAP},
    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "displacementTexture"_s, UFBX_MATERIAL_PBR_DISPLACEMENT_MAP, -1, MaterialExclusionGroup::Displacement},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "displacementFactor"_s, MaterialMapping::DisallowTexture(), UFBX_MATERIAL_PBR_DISPLACEMENT_MAP, -1, MaterialExclusionGroup::DisplacementFactor},

    {UfbxMaterialLayer::Matte, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_MATTE_FACTOR},
    {UfbxMaterialLayer::Matte, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_MATTE_COLOR},

    {UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "OcclusionTexture"_s, UFBX_MATERIAL_PBR_AMBIENT_OCCLUSION},
};

constexpr MaterialMapping materialMappingPbrFactor[]{
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "baseColorFactor"_s, {}, UFBX_MATERIAL_PBR_BASE_FACTOR},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularColorFactor"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_FACTOR, -1, MaterialExclusionGroup::SpecularFactor},
    {UfbxMaterialLayer::Base, MaterialAttributeType::Float, "emissiveColorFactor"_s, {}, UFBX_MATERIAL_PBR_EMISSION_FACTOR, -1, MaterialExclusionGroup::EmissionFactor},
};

}}}

#endif
