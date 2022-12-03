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
    MaterialAttributeType attributeType;
    Containers::StringView attribute;

    /* Override the attribute of the texture, defaults to attribute+"Texture" */
    Containers::StringView textureAttribute;

    Int valueMap;
    Int factorMap;

    MaterialExclusionGroup exclusionGroup;

    constexpr MaterialMapping(UfbxMaterialLayer layer, MaterialAttributeType attributeType, Containers::StringView attribute, Containers::StringView textureAttribute, Int valueMap, Int factorMap=-1, MaterialExclusionGroup exclusionGroup=MaterialExclusionGroup{})
        : layer(layer), attributeType(attributeType), attribute(attribute), textureAttribute(textureAttribute), valueMap(valueMap), factorMap(factorMap), exclusionGroup(exclusionGroup) { }
};

const constexpr MaterialMapping materialMappingFbx[] = {
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "DiffuseColor"_s, "DiffuseTexture"_s, UFBX_MATERIAL_FBX_DIFFUSE_COLOR, UFBX_MATERIAL_FBX_DIFFUSE_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "SpecularColor"_s, "SpecularTexture"_s, UFBX_MATERIAL_FBX_SPECULAR_COLOR, UFBX_MATERIAL_FBX_SPECULAR_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Shininess"_s, "shininessTexture"_s, UFBX_MATERIAL_FBX_SPECULAR_EXPONENT },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "reflectionColor"_s, "reflectionTexture"_s, UFBX_MATERIAL_FBX_REFLECTION_COLOR, UFBX_MATERIAL_FBX_REFLECTION_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "transparencyColor"_s, "transparencyTexture"_s, UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR, UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "EmissiveColor"_s, "EmissiveTexture"_s, UFBX_MATERIAL_FBX_EMISSION_COLOR, UFBX_MATERIAL_FBX_EMISSION_FACTOR, MaterialExclusionGroup::Emission },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "AmbientColor"_s, "AmbientTexture"_s, UFBX_MATERIAL_FBX_AMBIENT_COLOR, UFBX_MATERIAL_FBX_AMBIENT_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_FBX_NORMAL_MAP, -1, MaterialExclusionGroup::NormalTexture },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_FBX_BUMP, -1, MaterialExclusionGroup::NormalTexture },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "bumpTexture"_s, UFBX_MATERIAL_FBX_BUMP },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, {}, "bumpFactor"_s, UFBX_MATERIAL_FBX_BUMP_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "displacementTexture"_s, UFBX_MATERIAL_FBX_DISPLACEMENT, -1, MaterialExclusionGroup::Displacement },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, {}, "displacementFactor"_s, UFBX_MATERIAL_FBX_DISPLACEMENT_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "vectorDisplacementTexture"_s, UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, {}, "vectorDisplacementFactor"_s, UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT_FACTOR },
};

const constexpr MaterialMapping materialMappingPbr[] = {
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector4, "BaseColor"_s, {}, UFBX_MATERIAL_PBR_BASE_COLOR, UFBX_MATERIAL_PBR_BASE_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_ROUGHNESS },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Glossiness"_s, {}, UFBX_MATERIAL_PBR_GLOSSINESS },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "Metalness"_s, {}, UFBX_MATERIAL_PBR_METALNESS },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "diffuseRoughness"_s, {}, UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS },

    /* Specular "layer", it's not really a layer as it modifies the specular
       implicitly defined by BaseColor and Metalness */
    /* Note: This is semantically different from SpecularColor, which is the
       Phong specular, this is akin to KHR_materials_specular. I guess this
       could be also worded as "specularTint" */
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "specularColor"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_COLOR, UFBX_MATERIAL_PBR_SPECULAR_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularIor"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_IOR },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularAnisotropy"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "specularRotation"_s, {}, UFBX_MATERIAL_PBR_SPECULAR_ROTATION },

    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_COLOR },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "depth"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_DEPTH },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "scatter"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "scatterAnisotropy"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER_ANISOTROPY },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "dispersion"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_DISPERSION },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "roughness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_ROUGHNESS },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "glossiness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_GLOSSINESS },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Float, "extraRoughness"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_EXTRA_ROUGHNESS },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Long, "priority"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_PRIORITY },
    MaterialMapping{ UfbxMaterialLayer::Transmission, MaterialAttributeType::Bool, "enableInAov"_s, {}, UFBX_MATERIAL_PBR_TRANSMISSION_ENABLE_IN_AOV },

    MaterialMapping{ UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Subsurface, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_COLOR },
    MaterialMapping{ UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "radius"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_RADIUS },
    MaterialMapping{ UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "scale"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_SCALE },
    MaterialMapping{ UfbxMaterialLayer::Subsurface, MaterialAttributeType::Float, "anisotropy"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY },
    MaterialMapping{ UfbxMaterialLayer::Subsurface, MaterialAttributeType::Vector3, "tintColor"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_TINT_COLOR },
    MaterialMapping{ UfbxMaterialLayer::Subsurface, MaterialAttributeType::Long, "type"_s, {}, UFBX_MATERIAL_PBR_SUBSURFACE_TYPE },

    MaterialMapping{ UfbxMaterialLayer::Sheen, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_SHEEN_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Sheen, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_SHEEN_COLOR },
    MaterialMapping{ UfbxMaterialLayer::Sheen, MaterialAttributeType::Float, "roughness"_s, {}, UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS },

    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_COAT_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_COAT_COLOR },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "Roughness"_s, {}, UFBX_MATERIAL_PBR_COAT_ROUGHNESS },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "Glossiness"_s, {}, UFBX_MATERIAL_PBR_COAT_GLOSSINESS },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "ior"_s, {}, UFBX_MATERIAL_PBR_COAT_IOR },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "anisotropy"_s, {}, UFBX_MATERIAL_PBR_COAT_ANISOTROPY },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "rotation"_s, {}, UFBX_MATERIAL_PBR_COAT_ROTATION },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_PBR_COAT_NORMAL },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "affectBaseColor"_s, {}, UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_COLOR },
    MaterialMapping{ UfbxMaterialLayer::Coat, MaterialAttributeType::Float, "affectBaseRoughness"_s, {}, UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_ROUGHNESS },

    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "thinFilmThickness"_s, {}, UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "thinFilmIor"_s, {}, UFBX_MATERIAL_PBR_THIN_FILM_IOR },

    /* @todo This could be it's own layer */
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Vector3, "EmissiveColor"_s, "EmissiveTexture"_s, UFBX_MATERIAL_PBR_EMISSION_COLOR, UFBX_MATERIAL_PBR_EMISSION_FACTOR, MaterialExclusionGroup::Emission },

    /* Copied over to BaseColor.a but we need to keep this copy as it might have
       a texture connected to it */
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "opacity"_s, {}, UFBX_MATERIAL_PBR_OPACITY },

    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "indirectDiffuse"_s, {}, UFBX_MATERIAL_PBR_INDIRECT_DIFFUSE },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType::Float, "indirectSpecular"_s, {}, UFBX_MATERIAL_PBR_INDIRECT_SPECULAR },

    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "NormalTexture"_s, UFBX_MATERIAL_PBR_NORMAL_MAP, -1, MaterialExclusionGroup::NormalTexture },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "tangentTexture"_s, UFBX_MATERIAL_PBR_TANGENT_MAP },
    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "displacementTexture"_s, UFBX_MATERIAL_PBR_DISPLACEMENT_MAP, -1, MaterialExclusionGroup::Displacement },

    MaterialMapping{ UfbxMaterialLayer::Matte, MaterialAttributeType::Float, "LayerFactor"_s, {}, UFBX_MATERIAL_PBR_MATTE_FACTOR },
    MaterialMapping{ UfbxMaterialLayer::Matte, MaterialAttributeType::Vector3, "color"_s, {}, UFBX_MATERIAL_PBR_MATTE_COLOR },

    MaterialMapping{ UfbxMaterialLayer::Base, MaterialAttributeType{}, {}, "OcclusionTexture"_s, UFBX_MATERIAL_PBR_AMBIENT_OCCLUSION },
};

}}}
