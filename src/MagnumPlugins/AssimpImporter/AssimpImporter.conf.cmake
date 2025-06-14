depends=AnyImageImporter
provides=3dsImporter
provides=3mfImporter
provides=Ac3dImporter
provides=BlenderImporter
provides=BvhImporter
provides=CsmImporter
provides=ColladaImporter
provides=DirectXImporter
provides=DxfImporter
provides=FbxImporter
provides=GltfImporter
provides=IfcImporter
provides=IrrlichtImporter
provides=LightWaveImporter
provides=ModoImporter
provides=MilkshapeImporter
provides=ObjImporter
provides=OgreImporter
provides=OpenGexImporter
provides=StanfordImporter
provides=StlImporter
provides=TrueSpaceImporter
provides=UnrealImporter
# USD import is since 5.4.3, this gets commented out on older versions
${ASSIMP_USD_SUPPORT}provides=UsdImporter
provides=ValveImporter
provides=XglImporter

# [configuration_]
[configuration]
# Controls the workaround for an Assimp 4.1 "white ambient" bug
forceWhiteAmbientToBlack=true

# Optimize imported linearly-interpolated quaternion animation tracks to
# ensure shortest path is always chosen. This can be controlled separately
# for each animation import.
optimizeQuaternionShortestPath=true

# Normalize quaternion animation tracks, if they are not already.
# This can be controlled separately for each animation import.
normalizeQuaternions=true

# Merge all animations into a single clip. Useful for preserving cinematic
# animations when using the Blender glTF exporter, as it exports animation of
# every object as a separate clip. For more information see
# https://blender.stackexchange.com/q/5689 and
# https://github.com/KhronosGroup/glTF-Blender-Exporter/pull/166.
mergeAnimationClips=false

# Remove dummy animation tracks inserted by Assimp that contain only a single
# key/value pair with the default node transformation
removeDummyAnimationTracks=true

# Maximum amount of joint weights per vertex to import as vertex attributes.
# A value of 0 disables any limit, any other value implicitly enables
# aiProcess_LimitBoneWeights. This can be changed only before the first file
# is opened.
maxJointWeights=0

# Expose MeshAttribute::JointIds and Weights under custom "JOINTS" and
# "WEIGHTS" aliases for backwards compatibility with code that relies on
# those being present. If Magnum is built with MAGNUM_BUILD_DEPRECATED
# disabled, no aliases are provided and this option is ignored.
compatibilitySkinningAttributes=true

# Merge all skins into a single skin. Since Assimp exposes one skin for each
# mesh with joint weights, this is useful for getting a single skin covering
# all meshes in the file. This can only be controlled on a per-file basis.
mergeSkins=false

# AI_CONFIG_* values, can be changed only before the first file is opened
ImportColladaIgnoreUpDirection=false

# By default material attributes that aren't recognized as builtin attributes
# are imported with raw Assimp names and types. Enabling this option
# completely ignores unrecognized attributes instead.
ignoreUnrecognizedMaterialData=false

# Don't attempt to recognize builtin material attributes and always import
# them as raw material data (so e.g. instead of DiffuseColor you'll get
# $clr.diffuse). Implicitly disables ignoreUnrecognizedMaterialData. Mainly
# for testing purposes.
forceRawMaterialData=false

# aiPostProcessSteps, applied to each opened file
[configuration/postprocess]
CalcTangentSpace=false
JoinIdenticalVertices=true
Triangulate=true
GenNormals=false
GenSmoothNormals=false
SplitLargeMeshes=false
PreTransformVertices=false
# See the maxJointWeights option for LimitBoneWeights instead
ValidateDataStructure=false
ImproveCacheLocality=false
RemoveRedundantMaterials=false
FixInfacingNormals=false
SortByPType=true
FindDegenerates=false
FindInvalidData=false
GenUVCoords=false
TransformUVCoords=false
FindInstances=false
OptimizeMeshes=false
OptimizeGraph=false
FlipUVs=false
FlipWindingOrder=false
SplitByBoneCount=false
Debone=false
# [configuration_]
