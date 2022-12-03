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
    Custom,
};

constexpr UnsignedInt UfbxMaterialLayerCount = UnsignedInt(UfbxMaterialLayer::Custom);

const constexpr Containers::StringView ufbxMaterialLayerNames[] = {
    {},
    "Coat"_s,
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
    Displacement = 1 << 2,
};

typedef Containers::EnumSet<MaterialExclusionGroup> MaterialExclusionGroups;

struct MaterialMapping {
    UfbxMaterialLayer layer;
    MaterialAttributeType attributeType = {};
    Containers::StringView attribute;

    /* Override the attribute of the texture, defaults to attribute+"Texture" */
    Containers::StringView textureAttribute;

    Int valueMap = -1;
    Int factorMap = -1;

    MaterialExclusionGroup exclusionGroup = MaterialExclusionGroup{};
};

const constexpr MaterialMapping materialMappingFbx[] = {
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "DiffuseColor"_s, "DiffuseTexture"_s, UFBX_MATERIAL_FBX_DIFFUSE_COLOR, UFBX_MATERIAL_FBX_DIFFUSE_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "SpecularColor"_s, "SpecularTexture"_s, UFBX_MATERIAL_FBX_SPECULAR_COLOR, UFBX_MATERIAL_FBX_SPECULAR_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Shininess"_s, "shininessTexture"_s, UFBX_MATERIAL_FBX_SPECULAR_EXPONENT },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "reflectionColor"_s, "reflectionTexture"_s, UFBX_MATERIAL_FBX_REFLECTION_COLOR, UFBX_MATERIAL_FBX_REFLECTION_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "transparencyColor"_s, "transparencyTexture"_s, UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR, UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "EmissiveColor"_s, "EmissiveTexture"_s, UFBX_MATERIAL_FBX_EMISSION_COLOR, UFBX_MATERIAL_FBX_EMISSION_FACTOR, MaterialExclusionGroup::Emission },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "AmbientColor"_s, "AmbientTexture"_s, UFBX_MATERIAL_FBX_AMBIENT_COLOR, UFBX_MATERIAL_FBX_AMBIENT_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_FBX_NORMAL_MAP, -1, MaterialExclusionGroup::NormalTexture },
    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_FBX_BUMP, -1, MaterialExclusionGroup::NormalTexture },
    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "bumpTexture"_s, UFBX_MATERIAL_FBX_BUMP },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, {}, "bumpFactor"_s, UFBX_MATERIAL_FBX_BUMP_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "displacementTexture"_s, UFBX_MATERIAL_FBX_DISPLACEMENT, -1, MaterialExclusionGroup::Displacement },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, {}, "displacementFactor"_s, UFBX_MATERIAL_FBX_DISPLACEMENT_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "vectorDisplacementTexture"_s, UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, {}, "vectorDisplacementFactor"_s, UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT_FACTOR },
};

const constexpr MaterialMapping materialMappingPbr[] = {
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "BaseColor"_s, {}, UFBX_MATERIAL_PBR_BASE_COLOR, UFBX_MATERIAL_PBR_BASE_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_ROUGHNESS },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Glossiness"_s, {}, UFBX_MATERIAL_PBR_GLOSSINESS },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Metalness"_s, {}, UFBX_MATERIAL_PBR_METALNESS },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "diffuseRoughness"_s, {}, UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS },

    /* Specular "layer", it's not really a layer as it modifies the specular
       implicitly defined by BaseColor and Metalness */
    /* Note: This is semantically different from SpecularColor, which is the
       Phong specular, this is akin to KHR_materials_specular. I guess this
       could be also worded as "specularTint" */
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "specularColor"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_COLOR, UFBX_MATERIAL_PBR_SPECULAR_FACTOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularIor"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_IOR },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularAnisotropy"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularRotation"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_ROTATION },

    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_COLOR },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "depth"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_DEPTH },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "scatter"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "scatterAnisotropy"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER_ANISOTROPY },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "dispersion"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_DISPERSION },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "roughness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_ROUGHNESS },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "glossiness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_GLOSSINESS },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "extraRoughness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_EXTRA_ROUGHNESS },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Long, "priority"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_PRIORITY },
    { UfbxMaterialLayer::Transmission, MaterialAttributeType::Bool, "enableInAov"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_ENABLE_IN_AOV },

    { UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR },
    { UfbxMaterialLayer::Subsurface, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_COLOR },
    { UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "radius"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_RADIUS },
    { UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "scale"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_SCALE },
    { UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "anisotropy"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY },
    { UfbxMaterialLayer::Subsurface, MaterialAttributeType::Vector3, "tintColor"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_TINT_COLOR },
    { UfbxMaterialLayer::Subsurface, MaterialAttributeType::Long, "type"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_TYPE },

    { UfbxMaterialLayer::Sheen, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_SHEEN_FACTOR },
    { UfbxMaterialLayer::Sheen, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_SHEEN_COLOR },
    { UfbxMaterialLayer::Sheen, MaterialAttributeType::Float, "roughness"_s, {}, UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS },

    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_COAT_FACTOR },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_COAT_COLOR },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_COAT_ROUGHNESS },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "Glossiness"_s, {}, UFBX_MATERIAL_PBR_COAT_GLOSSINESS },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "ior"_s, {}, UFBX_MATERIAL_PBR_COAT_IOR },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "anisotropy"_s, {}, UFBX_MATERIAL_PBR_COAT_ANISOTROPY },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "rotation"_s, {}, UFBX_MATERIAL_PBR_COAT_ROTATION },
    { UfbxMaterialLayer::Coat, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_PBR_COAT_NORMAL },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "affectBaseColor"_s, {}, UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_COLOR },
    { UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "affectBaseRoughness"_s, {}, UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_ROUGHNESS },

    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "thinFilmThickness"_s, {}, UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "thinFilmIor"_s, {}, UFBX_MATERIAL_PBR_THIN_FILM_IOR },

    /* @todo This could be it's own layer */
    { UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "EmissiveColor"_s, "EmissiveTexture"_s, UFBX_MATERIAL_PBR_EMISSION_COLOR, UFBX_MATERIAL_PBR_EMISSION_FACTOR, MaterialExclusionGroup::Emission },

    /* @todo Should this be translated into BaseColor.a?
       It represents non-physical fade out in the PBR model. */
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "opacity"_s, {}, UFBX_MATERIAL_PBR_OPACITY },

    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "indirectDiffuse"_s, {}, UFBX_MATERIAL_PBR_INDIRECT_DIFFUSE },
    { UfbxMaterialLayer::Base, MaterialAttributeType::Float, "indirectSpecular"_s, {}, UFBX_MATERIAL_PBR_INDIRECT_SPECULAR },

    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_PBR_NORMAL_MAP, -1, MaterialExclusionGroup::NormalTexture },
    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "tangentTexture"_s, UFBX_MATERIAL_PBR_TANGENT_MAP },
    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "displacementTexture"_s, UFBX_MATERIAL_PBR_DISPLACEMENT_MAP, -1, MaterialExclusionGroup::Displacement },

    { UfbxMaterialLayer::Matte, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_MATTE_FACTOR },
    { UfbxMaterialLayer::Matte, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_MATTE_COLOR },

    { UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "OcclusionTexture"_s, UFBX_MATERIAL_PBR_AMBIENT_OCCLUSION },
};

}}}
