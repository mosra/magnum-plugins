/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2020 John Laxson <jlaxson@mac.com>

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

#include "StanfordImporter.h"

#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Corrade/Utility/String.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/MeshTools/Combine.h>
#include <Magnum/Trade/ArrayAllocator.h>
#include <Magnum/Trade/MeshData.h>

namespace Magnum { namespace Trade {

struct StanfordImporter::State {
    Containers::Array<char> data;
    std::size_t headerSize;
    Containers::Array<MeshAttributeData> attributeData;
    Containers::Array<MeshAttributeData> faceAttributeData;
    UnsignedInt vertexStride{}, vertexCount{}, faceIndicesOffset{}, faceSkip{}, faceCount{};
    MeshIndexType faceSizeType{}, faceIndexType{};
    bool fileFormatNeedsEndianSwapping;

    std::unordered_map<std::string, MeshAttribute> attributeNameMap;
    Containers::Array<std::string> attributeNames;
};

StanfordImporter::StanfordImporter() {
    /** @todo horrible workaround, fix this properly */
    configuration().setValue("perFaceToPerVertex", true);
    configuration().setValue("triangleFastPath", true);
    configuration().setValue("objectIdAttribute", "object_id");
}

StanfordImporter::StanfordImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

StanfordImporter::~StanfordImporter() = default;

ImporterFeatures StanfordImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool StanfordImporter::doIsOpened() const { return !!_state; }

void StanfordImporter::doClose() { _state = nullptr; }

void StanfordImporter::doOpenFile(const std::string& filename) {
    if(!Utility::Directory::exists(filename)) {
        Error{} << "Trade::StanfordImporter::openFile(): cannot open file" << filename;
        return;
    }

    openDataInternal(Utility::Directory::read(filename));
}

void StanfordImporter::doOpenData(Containers::ArrayView<const char> data) {
    Containers::Array<char> copy{NoInit, data.size()};
    Utility::copy(data, copy);
    openDataInternal(std::move(copy));
}

namespace {

enum class PropertyType {
    Vertex = 1,
    Face
};

MeshIndexType parseIndexType(const std::string& type) {
    if(type == "uchar"  || type == "uint8" ||
       type == "char"   || type == "int8")
        return MeshIndexType::UnsignedByte;
    if(type == "ushort" || type == "uint16" ||
       type == "short"  || type == "int16")
        return MeshIndexType::UnsignedShort;
    if(type == "uint"   || type == "uint32" ||
       type == "int"    || type == "int32")
        return MeshIndexType::UnsignedInt;

    return {};
}

VertexFormat parseAttributeType(const std::string& type) {
    if(type == "uchar"  || type == "uint8")
        return VertexFormat::UnsignedByte;
    if(type == "char"   || type == "int8")
        return VertexFormat::Byte;
    if(type == "ushort" || type == "uint16")
        return VertexFormat::UnsignedShort;
    if(type == "short"  || type == "int16")
        return VertexFormat::Short;
    if(type == "uint"   || type == "uint32")
        return VertexFormat::UnsignedInt;
    if(type == "int"    || type == "int32")
        return VertexFormat::Int;
    if(type == "float"  || type == "float32")
        return VertexFormat::Float;
    if(type == "double" || type == "float64")
        return VertexFormat::Double;

    return {};
}

template<class T, class U> inline T extractValue(const char* buffer, const bool endianSwap) {
    /* do a memcpy() instead of *reinterpret_cast, as that'll correctly handle
       unaligned loads as well */
    U dest;
    std::memcpy(&dest, buffer, sizeof(U));
    if(endianSwap) Utility::Endianness::swapInPlace(dest);
    return T(dest);
}

template<class T> T extractIndexValue(const char* buffer, const MeshIndexType type, const bool endianSwap) {
    switch(type) {
        /* LCOV_EXCL_START */
        #define _c(type) case MeshIndexType::type: return extractValue<T, type>(buffer, endianSwap);
        _c(UnsignedByte)
        _c(UnsignedShort)
        _c(UnsignedInt)
        #undef _c
        /* LCOV_EXCL_STOP */

        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }
}

std::string extractLine(Containers::ArrayView<const char>& in) {
    for(const char& i: in) if(i == '\n') {
        std::size_t end = &i - in;
        auto out = in.prefix(end);
        in = in.suffix(end + 1);
        return {out.begin(), out.end()};
    }

    auto out = in;
    in = {};
    return {out.begin(), out.end()};
}

template<std::size_t size> bool checkVectorAttributeValidity(const Math::Vector<size, VertexFormat>& formats, const Math::Vector<size, UnsignedInt>& offsets, const char* name) {
    /* Check that we have the same type for all position coordinates */
    if(formats != Math::Vector<size, VertexFormat>{formats[0]}) {
        Error{} << "Trade::StanfordImporter::openData(): expecting all" << name << "components to have the same type but got" << formats;
        return false;
    }

    /* And that they are right after each other in correct order */
    const UnsignedInt formatSize = vertexFormatSize(formats[0]);
    for(std::size_t i = 1; i != size; ++i) {
        if(offsets[i] != offsets[i - 1] + formatSize) {
            Error{} << "Trade::StanfordImporter::openData(): expecting" << name << "components to be tightly packed, but got offsets" << offsets << "for a" << formatSize << Debug::nospace << "-byte type";
            return false;
        }
    }

    return true;
}

}

void StanfordImporter::openDataInternal(Containers::Array<char>&& data) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.empty()) {
        Error{} << "Trade::StanfordImporter::openData(): the file is empty";
        return;
    }

    /* Initialize the state */
    auto state = Containers::pointer<State>();
    Containers::ArrayView<const char> in = data;

    /* Check file signature */
    {
        std::string header = Utility::String::rtrim(extractLine(in));
        if(header != "ply") {
            Error{} << "Trade::StanfordImporter::openData(): invalid file signature" << header;
            return;
        }
    }

    /* Parse format line */
    Containers::Optional<bool> fileFormatNeedsEndianSwapping;
    {
        while(in) {
            const std::string line = extractLine(in);
            std::vector<std::string> tokens = Utility::String::splitWithoutEmptyParts(line);

            /* Skip empty lines and comments */
            if(tokens.empty() || tokens.front() == "comment")
                continue;

            if(tokens[0] != "format") {
                Error{} << "Trade::StanfordImporter::openData(): expected format line, got" << line;
                return;
            }

            if(tokens.size() != 3) {
                Error() << "Trade::StanfordImporter::openData(): invalid format line" << line;
                return;
            }

            if(tokens[2] == "1.0") {
                if(tokens[1] == "binary_little_endian") {
                    fileFormatNeedsEndianSwapping = Utility::Endianness::isBigEndian();
                    break;
                } else if(tokens[1] == "binary_big_endian") {
                    fileFormatNeedsEndianSwapping = !Utility::Endianness::isBigEndian();
                    break;
                }
            }

            Error{} << "Trade::StanfordImporter::openData(): unsupported file format" << tokens[1] << tokens[2];
            return;
        }
    }

    /* Check format line consistency */
    if(!fileFormatNeedsEndianSwapping) {
        Error{} << "Trade::StanfordImporter::openData(): missing format line";
        return;
    }
    state->fileFormatNeedsEndianSwapping = *fileFormatNeedsEndianSwapping;

    /* Parse rest of the header */
    Math::Vector3<VertexFormat> positionFormats;
    Math::Vector3<VertexFormat> normalFormats;
    Math::Vector2<VertexFormat> textureCoordinateFormats;
    Math::Vector4<VertexFormat> colorFormats;
    VertexFormat objectIdFormat{};
    Vector3ui positionOffsets{~UnsignedInt{}};
    Vector3ui normalOffsets{~UnsignedInt{}};
    Vector2ui textureCoordinateOffsets{~UnsignedInt{}};
    Vector4ui colorOffsets{~UnsignedInt{}};
    UnsignedInt objectIdOffset = ~UnsignedInt{};
    bool perFaceNormals = false;
    bool perFaceColors = false;
    bool perFaceObjectIds = false;
    {
        std::size_t vertexComponentOffset{};
        PropertyType propertyType{};
        while(in) {
            const std::string line = extractLine(in);
            std::vector<std::string> tokens = Utility::String::splitWithoutEmptyParts(line);

            /* Skip empty lines and comments */
            if(tokens.empty() || tokens.front() == "comment")
                continue;

            /* Elements */
            if(tokens[0] == "element") {
                /* Vertex elements */
                if(tokens.size() == 3 && tokens[1] == "vertex") {
                    state->vertexCount = std::stoi(tokens[2]);
                    propertyType = PropertyType::Vertex;

                /* Face elements */
                } else if(tokens.size() == 3 &&tokens[1] == "face") {
                    state->faceCount = std::stoi(tokens[2]);
                    propertyType = PropertyType::Face;

                /* Something else */
                } else {
                    Error{} << "Trade::StanfordImporter::openData(): unknown element" << tokens[1];
                    return;
                }

            /* Element properties */
            } else if(tokens[0] == "property") {
                /* Vertex element properties */
                if(propertyType == PropertyType::Vertex) {
                    if(tokens.size() != 3) {
                        Error{} << "Trade::StanfordImporter::openData(): invalid vertex property line" << line;
                        return;
                    }

                    /* Component type */
                    const VertexFormat componentFormat = parseAttributeType(tokens[1]);
                    if(componentFormat == VertexFormat{}) {
                        Error{} << "Trade::StanfordImporter::openData(): invalid vertex component type" << tokens[1];
                        return;
                    }

                    /* Component */
                    if(tokens[2] == "x") {
                        positionOffsets.x() = vertexComponentOffset;
                        positionFormats.x() = componentFormat;
                    } else if(tokens[2] == "y") {
                        positionOffsets.y() = vertexComponentOffset;
                        positionFormats.y() = componentFormat;
                    } else if(tokens[2] == "z") {
                        positionOffsets.z() = vertexComponentOffset;
                        positionFormats.z() = componentFormat;
                    } else if(tokens[2] == "nx") {
                        normalOffsets.x() = vertexComponentOffset;
                        normalFormats.x() = componentFormat;
                    } else if(tokens[2] == "ny") {
                        normalOffsets.y() = vertexComponentOffset;
                        normalFormats.y() = componentFormat;
                    } else if(tokens[2] == "nz") {
                        normalOffsets.z() = vertexComponentOffset;
                        normalFormats.z() = componentFormat;
                    /* LuxBlend uses s/t, Mitsuba uses u/v */
                    } else if(tokens[2] == "u" || tokens[2] == "s") {
                        textureCoordinateOffsets.x() = vertexComponentOffset;
                        textureCoordinateFormats.x() = componentFormat;
                    } else if(tokens[2] == "v" || tokens[2] == "t") {
                        textureCoordinateOffsets.y() = vertexComponentOffset;
                        textureCoordinateFormats.y() = componentFormat;
                    } else if(tokens[2] == "red") {
                        colorOffsets.x() = vertexComponentOffset;
                        colorFormats.x() = componentFormat;
                    } else if(tokens[2] == "green") {
                        colorOffsets.y() = vertexComponentOffset;
                        colorFormats.y() = componentFormat;
                    } else if(tokens[2] == "blue") {
                        colorOffsets.z() = vertexComponentOffset;
                        colorFormats.z() = componentFormat;
                    /* Several people complain that Meshlab doesn't support
                       alpha, so let's make sure we do :P
                       https://github.com/cnr-isti-vclab/meshlab/issues/161*/
                    } else if(tokens[2] == "alpha") {
                        colorOffsets.w() = vertexComponentOffset;
                        colorFormats.w() = componentFormat;
                    } else if(tokens[2] == configuration().value("objectIdAttribute")) {
                        objectIdOffset = vertexComponentOffset;
                        objectIdFormat = componentFormat;

                    /* Unknown component, add to the attribute list. Stride is
                       not known yet, using 0 until it's updated later. */
                    } else {
                        auto inserted = state->attributeNameMap.emplace(tokens[2],
                            meshAttributeCustom(state->attributeNames.size()));
                        arrayAppend(state->attributeNames, tokens[2]);
                        arrayAppend(state->attributeData, MeshAttributeData{
                            inserted.first->second,
                            componentFormat,
                            vertexComponentOffset, state->vertexCount, 0});
                    }

                    /* Add size of current component to total offset */
                    vertexComponentOffset += vertexFormatSize(componentFormat);

                /* Face element properties */
                } else if(propertyType == PropertyType::Face) {
                    /* Face vertex indices. The vertex_indices name is usual,
                       Assimp exports with vertex_index, reference from
                       http://paulbourke.net/dataformats/ply/ mentions both. */
                    if(tokens.size() == 5 && tokens[1] == "list" && (tokens[4] == "vertex_indices" || tokens[4] == "vertex_index")) {
                        state->faceIndicesOffset = state->faceSkip;
                        state->faceSkip = 0;

                        /* Face size type */
                        if((state->faceSizeType = parseIndexType(tokens[2])) == MeshIndexType{}) {
                            Error{} << "Trade::StanfordImporter::openData(): invalid face size type" << tokens[2];
                            return;
                        }

                        /* Face index type */
                        if((state->faceIndexType = parseIndexType(tokens[3])) == MeshIndexType{}) {
                            Error{} << "Trade::StanfordImporter::openData(): invalid face index type" << tokens[3];
                            return;
                        }

                    /* Per-face component */
                    } else if(tokens.size() == 3) {
                       const VertexFormat componentFormat = parseAttributeType(tokens[1]);
                        if(componentFormat == VertexFormat{}) {
                            Error{} << "Trade::StanfordImporter::openData(): invalid face component type" << tokens[1];
                            return;
                        }

                        /* Before indices are found, faceIndicesOffset is zero.
                           After indices are found, faceIndicesOffset is set
                           and faceSkip is zero again, thus the sum of the two
                           is always offset from the beginning of the face,
                           which is what we need. */
                        const UnsignedInt faceComponentOffset =
                            state->faceIndicesOffset + state->faceSkip;

                        /* Per-face normals and colors make sense, OTOH
                           positions or texture coordinates don't, so not
                           handling those in any way (they would appear as
                           custom attributes) */
                        if(tokens[2] == "nx") {
                            perFaceNormals = true;
                            normalOffsets.x() = faceComponentOffset;
                            normalFormats.x() = componentFormat;
                        } else if(tokens[2] == "ny") {
                            normalOffsets.y() = faceComponentOffset;
                            normalFormats.y() = componentFormat;
                        } else if(tokens[2] == "nz") {
                            normalOffsets.z() = faceComponentOffset;
                            normalFormats.z() = componentFormat;
                        } else if(tokens[2] == "red") {
                            perFaceColors = true;
                            colorOffsets.x() = faceComponentOffset;
                            colorFormats.x() = componentFormat;
                        } else if(tokens[2] == "green") {
                            colorOffsets.y() = faceComponentOffset;
                            colorFormats.y() = componentFormat;
                        } else if(tokens[2] == "blue") {
                            colorOffsets.z() = faceComponentOffset;
                            colorFormats.z() = componentFormat;
                        } else if(tokens[2] == "alpha") {
                            colorOffsets.w() = faceComponentOffset;
                            colorFormats.w() = componentFormat;
                        } else if(tokens[2] == configuration().value("objectIdAttribute")) {
                            perFaceObjectIds = true;
                            objectIdOffset = faceComponentOffset;
                            objectIdFormat = componentFormat;

                        /* Unknown component, add to the face attribute list.
                           Stride and actual triangle face count is not known yet,
                           using 0 until it's updated later. */
                        } else {
                            auto inserted = state->attributeNameMap.emplace(tokens[2],
                                meshAttributeCustom(state->attributeNames.size()));
                            arrayAppend(state->attributeNames, tokens[2]);
                            arrayAppend(state->faceAttributeData, MeshAttributeData{
                                inserted.first->second,
                                componentFormat, faceComponentOffset, 0, 0});
                        }

                        state->faceSkip += vertexFormatSize(componentFormat);

                    /* Fail on unknown lines */
                    } else {
                        Error{} << "Trade::StanfordImporter::openData(): invalid face property line" << line;
                        return;
                    }

                /* Unexpected property line */
                } else if(propertyType == PropertyType{}) {
                    Error{} << "Trade::StanfordImporter::openData(): unexpected property line";
                    return;
                }

            /* Header end */
            } else if(tokens[0] == "end_header") {
                break;

            /* Something else */
            } else {
                Error{} << "Trade::StanfordImporter::openData(): unknown line" << line;
                return;
            }
        }

        state->vertexStride = vertexComponentOffset;
    }

    /* Check header consistency */
    if((positionOffsets >= Vector3ui{~UnsignedInt{}}).any()) {
        Error{} << "Trade::StanfordImporter::openData(): incomplete vertex specification";
        return;
    }
    if(state->faceSizeType == MeshIndexType{} || state->faceIndexType == MeshIndexType{}) {
        Error{} << "Trade::StanfordImporter::openData(): incomplete face specification";
        return;
    }

    /* Stride is known now, update it in custom attributes. Triangle face count
       is not known yet, that'll get updated after parsing all faces. */
    for(MeshAttributeData& attribute: state->attributeData) {
        attribute = MeshAttributeData{
            attribute.name(), attribute.format(),
            attribute.offset({}), state->vertexCount, std::ptrdiff_t(state->vertexStride)};
    }
    for(MeshAttributeData& attribute: state->faceAttributeData) {
        attribute = MeshAttributeData{
            attribute.name(), attribute.format(),
            attribute.offset({}), 0, std::ptrdiff_t(state->faceIndicesOffset + state->faceSkip)};
    }

    /* Wrap up positions */
    {
        /* Check that all components have the same type and right after each
           other */
        if(!checkVectorAttributeValidity(positionFormats, positionOffsets, "position"))
            return;

        /* Ensure the type is one of allowed */
        if(positionFormats.x() != VertexFormat::Float &&
           positionFormats.x() != VertexFormat::UnsignedByte &&
           positionFormats.x() != VertexFormat::Byte &&
           positionFormats.x() != VertexFormat::UnsignedShort &&
           positionFormats.x() != VertexFormat::Short) {
            Error{} << "Trade::StanfordImporter::openData(): unsupported position component type" << positionFormats.x();
            return;
        }

        /* Add the attribute */
        arrayAppend(state->attributeData, InPlaceInit,
            MeshAttribute::Position,
            vertexFormat(positionFormats.x(), 3, false),
            positionOffsets.x(), state->vertexCount, std::ptrdiff_t(state->vertexStride));
    }

    /* Wrap up normals, if any */
    if((normalOffsets < Vector3ui{~UnsignedInt{}}).any()) {
        /* Check that all components have the same type and right after each
           other */
        if(!checkVectorAttributeValidity(normalFormats, normalOffsets, "normal"))
            return;

        /* Ensure the type is one of allowed */
        if(normalFormats.x() != VertexFormat::Float &&
           normalFormats.x() != VertexFormat::Byte &&
           normalFormats.x() != VertexFormat::Short) {
            Error{} << "Trade::StanfordImporter::openData(): unsupported normal component type" << normalFormats.x();
            return;
        }

        /* Add the attribute. If it is per-face, actual triangle face count is
           not known yet, using 0 until after all faces are parsed. */
        if(!perFaceNormals) arrayAppend(state->attributeData,
            InPlaceInit, MeshAttribute::Normal,
            /* We want integer types normalized */
            vertexFormat(normalFormats.x(), 3, normalFormats.x() != VertexFormat::Float),
            normalOffsets.x(), state->vertexCount, std::ptrdiff_t(state->vertexStride));
        else arrayAppend(state->faceAttributeData,
            InPlaceInit, MeshAttribute::Normal,
            /* We want integer types normalized */
            vertexFormat(normalFormats.x(), 3, normalFormats.x() != VertexFormat::Float),
            normalOffsets.x(), 0u, std::ptrdiff_t(state->faceIndicesOffset + state->faceSkip));
    }

    /* Wrap up texture coordinates, if any */
    if((textureCoordinateOffsets < Vector2ui{~UnsignedInt{}}).any()) {
        /* Check that all components have the same type and right after each
           other */
        if(!checkVectorAttributeValidity(textureCoordinateFormats, textureCoordinateOffsets, "texture coordinate"))
            return;

        /* Ensure the type is one of allowed */
        if(textureCoordinateFormats.x() != VertexFormat::Float &&
           textureCoordinateFormats.x() != VertexFormat::UnsignedByte &&
           textureCoordinateFormats.x() != VertexFormat::UnsignedShort) {
            Error{} << "Trade::StanfordImporter::openData(): unsupported texture coordinate component type" << textureCoordinateFormats.x();
            return;
        }

        /* Add the attribute */
        arrayAppend(state->attributeData, InPlaceInit,
            MeshAttribute::TextureCoordinates,
            /* We want integer types normalized */
            vertexFormat(textureCoordinateFormats.x(), 2, textureCoordinateFormats.x() != VertexFormat::Float),
            textureCoordinateOffsets.x(), state->vertexCount, std::ptrdiff_t(state->vertexStride));
    }

    /* Wrap up colors, if any */
    if((colorOffsets < Vector4ui{~UnsignedInt{}}).any()) {
        /* Check that all components have the same type and right after each
           other. Alpha is optional. */
        if(colorFormats.w() == VertexFormat{}) {
            if(!checkVectorAttributeValidity(colorFormats.xyz(), colorOffsets.xyz(), "color"))
                return;
        } else {
            if(!checkVectorAttributeValidity(colorFormats, colorOffsets, "color"))
                return;
        }

        /* Ensure the type is one of allowed */
        if(colorFormats.x() != VertexFormat::Float &&
           colorFormats.x() != VertexFormat::UnsignedByte &&
           colorFormats.x() != VertexFormat::UnsignedShort) {
            Error{} << "Trade::StanfordImporter::openData(): unsupported color component type" << colorFormats.x();
            return;
        }

        /* Add the attribute. If it is per-face, actual triangle face count is
           not known yet, using 0 until after all faces are parsed. */
        if(!perFaceColors) arrayAppend(state->attributeData,
            InPlaceInit, MeshAttribute::Color,
            /* We want integer types normalized, 3 or 4 components */
            vertexFormat(colorFormats.x(), colorFormats.w() == VertexFormat{} ? 3 : 4, colorFormats.x() != VertexFormat::Float),
            colorOffsets.x(), state->vertexCount, std::ptrdiff_t(state->vertexStride));
        else arrayAppend(state->faceAttributeData,
            InPlaceInit, MeshAttribute::Color,
            /* We want integer types normalized, 3 or 4 components */
            vertexFormat(colorFormats.x(), colorFormats.w() == VertexFormat{} ? 3 : 4, colorFormats.x() != VertexFormat::Float),
            colorOffsets.x(), 0u, std::ptrdiff_t(state->faceIndicesOffset + state->faceSkip));
    }

    /* Wrap up object IDs, if any */
    if(objectIdOffset < ~UnsignedInt{}) {
        /* Same as with indices, various datasets in the wild use signed
           integers. Interpret them as unsigned. */
        VertexFormat format;
        if(objectIdFormat == VertexFormat::UnsignedInt || objectIdFormat == VertexFormat::Int)
            format = VertexFormat::UnsignedInt;
        else if(objectIdFormat == VertexFormat::UnsignedShort || objectIdFormat == VertexFormat::Short)
            format = VertexFormat::UnsignedShort;
        else if(objectIdFormat == VertexFormat::UnsignedByte || objectIdFormat == VertexFormat::Byte)
            format = VertexFormat::UnsignedByte;
        else {
            Error{} << "Trade::StanfordImporter::openData(): unsupported object ID type" << objectIdFormat;
            return;
        }

        /* Add the attribute. If it is per-face, actual triangle face count is
           not known yet, using 0 until after all faces are parsed. */
        if(!perFaceObjectIds) arrayAppend(state->attributeData,
            InPlaceInit, MeshAttribute::ObjectId, format,
            objectIdOffset, state->vertexCount, std::ptrdiff_t(state->vertexStride));
        else arrayAppend(state->faceAttributeData,
            InPlaceInit, MeshAttribute::ObjectId, format,
            objectIdOffset, 0u, std::ptrdiff_t(state->faceIndicesOffset + state->faceSkip));
    }

    if(in.size() < state->vertexStride*state->vertexCount) {
        Error{} << "Trade::StanfordImporter::openData(): incomplete vertex data";
        return;
    }

    /* All good, move the data to the state struct and save it. Remember header
       size so we can directly access the binary data in doMesh(). */
    state->data = std::move(data);
    state->headerSize = state->data.size() - in.size();
    _state = std::move(state);
}

UnsignedInt StanfordImporter::doMeshCount() const { return 1; }

UnsignedInt StanfordImporter::doMeshLevelCount(UnsignedInt) {
    return configuration().value<bool>("perFaceToPerVertex") ? 1 : 2;
}

Containers::Optional<MeshData> StanfordImporter::doMesh(UnsignedInt, const UnsignedInt level) {
    /* We either have per-face in the second level or we convert them to
       per-vertex, never both */
    CORRADE_INTERNAL_ASSERT(!(level == 1 && configuration().value<bool>("perFaceToPerVertex")));
    const bool parsePerFaceAttributes = level == 1 ||
        configuration().value<bool>("perFaceToPerVertex");

    Containers::ArrayView<const char> in = _state->data.suffix(_state->headerSize);

    /* Copy all vertex data */
    Containers::Array<char> vertexData;
    if(level == 0) {
        vertexData = Containers::Array<char>{NoInit,
        _state->vertexStride*_state->vertexCount};
        Utility::copy(in.prefix(vertexData.size()), vertexData);
    }
    in = in.suffix(_state->vertexStride*_state->vertexCount);

    /* Parse faces, keeping the original index type */
    Containers::Array<char> faceData;
    Containers::Array<char> indexData;
    const UnsignedInt faceIndexTypeSize = meshIndexTypeSize(_state->faceIndexType);
    const UnsignedInt faceSizeTypeSize = meshIndexTypeSize(_state->faceSizeType);
    UnsignedInt triangleFaceCount = _state->faceCount;

    /* Fast path -- if all faces are triangles, we can just copy all indices
       and per-face data directly without parsing anything */
    if(configuration().value<bool>("triangleFastPath") && in.size() == _state->faceCount*(_state->faceIndicesOffset + faceSizeTypeSize + 3*faceIndexTypeSize + _state->faceSkip)) {
        if(level == 0) {
            indexData = Containers::Array<char>{NoInit,
                _state->faceCount*3*faceIndexTypeSize};
            Containers::StridedArrayView2D<const char> src{in,
                in + _state->faceIndicesOffset + faceSizeTypeSize,
                {_state->faceCount, 3*faceIndexTypeSize},
                {std::ptrdiff_t(_state->faceIndicesOffset + faceSizeTypeSize + 3*faceIndexTypeSize + _state->faceSkip), 1}};
            Containers::StridedArrayView2D<char> dst{indexData,
                {_state->faceCount, 3*faceIndexTypeSize}};
            Utility::copy(src, dst);
        }

        if(parsePerFaceAttributes) {
            faceData = Containers::Array<char>{NoInit,
                _state->faceCount*(_state->faceIndicesOffset + _state->faceSkip)};
            Containers::StridedArrayView2D<const char> src{in,
                {_state->faceCount, _state->faceIndicesOffset + faceSizeTypeSize + 3*faceIndexTypeSize + _state->faceSkip}};
            Containers::StridedArrayView2D<char> dst{faceData,
                {_state->faceCount, _state->faceIndicesOffset + _state->faceSkip}};
            /* Separately copy the part before indices, and the part after.
               Transpose the sliced array so first dimension is faces and
               second bytes to avoid copying it byte-by-byte. */
            Utility::copy(
                src.prefix({_state->faceCount, _state->faceIndicesOffset}),
                dst.prefix({_state->faceCount, _state->faceIndicesOffset}));
            Utility::copy(
                src.suffix({0, _state->faceIndicesOffset + faceSizeTypeSize + 3*faceIndexTypeSize}),
                dst.suffix({0, _state->faceIndicesOffset}));
        }

    /* Otherwise reserve optimistically amount for all-triangle faces, and let
       the array grow */
    /** @todo the size could be estimated *exactly* via the above equation
       (assuming no stray data at EOF) */
    } else {
        if(parsePerFaceAttributes) Containers::arrayReserve<ArrayAllocator>(faceData,
            _state->faceCount*(_state->faceIndicesOffset + _state->faceSkip));
        Containers::arrayReserve<ArrayAllocator>(indexData,
            _state->faceCount*3*faceIndexTypeSize);
        for(std::size_t i = 0; i != _state->faceCount; ++i) {
            if(in.size() < _state->faceIndicesOffset + faceSizeTypeSize) {
                Error() << "Trade::StanfordImporter::mesh(): incomplete index data";
                return Containers::NullOpt;
            }

            /* Copy all face attributes that are before the index */
            const Containers::ArrayView<const char> faceDataBeforeIndices = in.prefix(_state->faceIndicesOffset);
            in = in.suffix(_state->faceIndicesOffset);

            /* Get face size */
            const Containers::ArrayView<const char> faceSizeData = in.prefix(faceSizeTypeSize);
            in = in.suffix(faceSizeTypeSize);
            const UnsignedInt faceSize = extractIndexValue<UnsignedInt>(faceSizeData, _state->faceSizeType, _state->fileFormatNeedsEndianSwapping);
            if(faceSize < 3 || faceSize > 4) {
                Error() << "Trade::StanfordImporter::mesh(): unsupported face size" << faceSize;
                return Containers::NullOpt;
            }

            /* Parse face indices */
            if(in.size() < faceIndexTypeSize*faceSize + _state->faceSkip) {
                Error() << "Trade::StanfordImporter::mesh(): incomplete face data";
                return Containers::NullOpt;
            }

            const Containers::ArrayView<const char> faceIndexData = in.prefix(faceIndexTypeSize*faceSize);
            in = in.suffix(faceIndexTypeSize*faceSize);
            const Containers::ArrayView<const char> faceDataAfterIndices = in.prefix(_state->faceSkip);
            in = in.suffix(_state->faceSkip);

            /* Append either the triangle or the first triangle of the quad */
            Containers::arrayAppend<ArrayAllocator>(indexData,
                faceIndexData.prefix(3*faceIndexTypeSize));
            if(parsePerFaceAttributes) {
                Containers::arrayAppend<ArrayAllocator>(faceData, faceDataBeforeIndices);
                Containers::arrayAppend<ArrayAllocator>(faceData, faceDataAfterIndices);
            }
            /* For a quad add the 0, 2 and 3 indices forming another triangle */
            if(faceSize == 4) {
                /* 0 0---3
                   |\ \  |
                   | \ \ |
                   |  \ \|
                   1---2 2 */
                Containers::arrayAppend<ArrayAllocator>(indexData,
                    faceIndexData.slice(0*faceIndexTypeSize, 1*faceIndexTypeSize));
                Containers::arrayAppend<ArrayAllocator>(indexData,
                    faceIndexData.slice(2*faceIndexTypeSize, 3*faceIndexTypeSize));
                Containers::arrayAppend<ArrayAllocator>(indexData,
                    faceIndexData.slice(3*faceIndexTypeSize, 4*faceIndexTypeSize));
                if(parsePerFaceAttributes) {
                    Containers::arrayAppend<ArrayAllocator>(faceData, faceDataBeforeIndices);
                    Containers::arrayAppend<ArrayAllocator>(faceData, faceDataAfterIndices);
                }
                ++triangleFaceCount;
            }
        }
    }

    /* We need to copy the attribute data (also because they use a forbidden
       deleter), so use that opportunity to also turn them from offset-only to
       absolute, and for per-face ones fill the count for each (which wasn't
       known until now) */
    Containers::Array<MeshAttributeData> vertexAttributeData;
    Containers::Array<MeshAttributeData> faceAttributeData;
    if(level == 0) {
        vertexAttributeData = Containers::Array<MeshAttributeData>{_state->attributeData.size()};
        for(std::size_t i = 0; i != vertexAttributeData.size(); ++i) {
            vertexAttributeData[i] = MeshAttributeData{
                _state->attributeData[i].name(),
                _state->attributeData[i].format(),
                _state->attributeData[i].data(vertexData)};
        }
    }

    if(parsePerFaceAttributes) {
        faceAttributeData = Containers::Array<MeshAttributeData>{_state->faceAttributeData.size()};
        for(std::size_t i = 0; i != faceAttributeData.size(); ++i) {
            faceAttributeData[i] = MeshAttributeData{
                _state->faceAttributeData[i].name(),
                _state->faceAttributeData[i].format(),
                Containers::StridedArrayView1D<const void>{
                    faceData,
                    _state->faceAttributeData[i].data(faceData).data(),
                    triangleFaceCount,
                    _state->faceAttributeData[i].stride()}};
        }
    }

    /* Endian-swap the data, if needed */
    if(_state->fileFormatNeedsEndianSwapping) {
        for(const auto& attributeData: {
            Containers::arrayView(vertexAttributeData),
            Containers::arrayView(faceAttributeData)}) {
            for(const MeshAttributeData& attribute: attributeData) {
                const UnsignedInt formatSize =
                    vertexFormatSize(vertexFormatComponentFormat(attribute.format()));
                if(formatSize == 1) continue;
                const UnsignedInt componentCount =
                    vertexFormatComponentCount(attribute.format());
                const Containers::StridedArrayView1D<const void> data =
                    attribute.data(attributeData.data() == vertexAttributeData.data() ? vertexData : faceData);
                /** @todo some arrayConstCast? ugh */
                const Containers::StridedArrayView1D<void> mutableData{
                    {const_cast<void*>(data.data()), ~std::size_t{}},
                    const_cast<void*>(data.data()), data.size(), data.stride()};
                if(formatSize == 2) {
                    for(Containers::StridedArrayView1D<UnsignedShort> component: Containers::arrayCast<2, UnsignedShort>(mutableData, componentCount).transposed<0, 1>())
                        Utility::Endianness::swapInPlace(component);
                } else if(formatSize == 4) {
                    for(Containers::StridedArrayView1D<UnsignedInt> component: Containers::arrayCast<2, UnsignedInt>(mutableData, componentCount).transposed<0, 1>())
                        Utility::Endianness::swapInPlace(component);
                } else if(formatSize == 8) {
                    for(Containers::StridedArrayView1D<UnsignedLong> component: Containers::arrayCast<2, UnsignedLong>(mutableData, componentCount).transposed<0, 1>())
                        Utility::Endianness::swapInPlace(component);
                } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            }
        }

        if(level == 0) {
            if(faceIndexTypeSize == 2)
                Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedShort>(indexData));
            else if(faceIndexTypeSize == 4)
                Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedInt>(indexData));
            else CORRADE_INTERNAL_ASSERT(faceIndexTypeSize == 1);
        }
    }

    /* Turn per-face attributes into per-vertex, if desired (and if there are
       any) */
    if(level == 0 && configuration().value<bool>("perFaceToPerVertex") && !faceAttributeData.empty()) {
        if(flags() & ImporterFlag::Verbose)
            Debug{} << "Trade::StanfordImporter::mesh(): converting" << faceAttributeData.size() << "per-face attributes to per-vertex";

        /** @todo in this case it'll assert if indices are out of bounds, check
            for it at runtime somehow */
        MeshIndexData indices{_state->faceIndexType, indexData};
        MeshData perVertex{MeshPrimitive::Triangles,
            std::move(indexData), indices,
            std::move(vertexData), std::move(vertexAttributeData)};
        MeshData perFace{MeshPrimitive::Faces,
            std::move(faceData), std::move(faceAttributeData), triangleFaceCount};
        return MeshTools::combineFaceAttributes(perVertex, perFace);
    }

    if(level == 0) {
        MeshIndexData indices{_state->faceIndexType, indexData};
        return MeshData{MeshPrimitive::Triangles,
            std::move(indexData), indices,
            std::move(vertexData), std::move(vertexAttributeData)};
    } else {
        return MeshData{MeshPrimitive::Faces,
            std::move(faceData), std::move(faceAttributeData), triangleFaceCount};
    }
}

std::string StanfordImporter::doMeshAttributeName(UnsignedShort name) {
    return _state && name < _state->attributeNames.size() ?
        _state->attributeNames[name] : "";
}

MeshAttribute StanfordImporter::doMeshAttributeForName(const std::string& name) {
    return _state ? _state->attributeNameMap[name] : MeshAttribute{};
}

}}

CORRADE_PLUGIN_REGISTER(StanfordImporter, Magnum::Trade::StanfordImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
