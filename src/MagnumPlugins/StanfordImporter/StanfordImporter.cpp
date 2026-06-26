/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
              Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/Containers/StringStlHash.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/MeshTools/Combine.h>
#include <Magnum/Trade/ArrayAllocator.h>
#include <Magnum/Trade/MeshData.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

struct StanfordImporter::State {
    Containers::Array<char> data;
    std::size_t headerSize;
    Containers::Array<MeshAttributeData> attributeData;
    Containers::Array<MeshAttributeData> faceAttributeData;
    UnsignedInt vertexStride{}, vertexCount{}, faceIndicesOffset{}, faceSkip{}, faceCount{};
    MeshIndexType faceSizeType{}, faceIndexType{};
    bool fileFormatNeedsEndianSwapping;

    std::unordered_map<Containers::StringView, MeshAttribute> attributeNameMap;
    Containers::Array<Containers::StringView> attributeNames;
};

#ifdef MAGNUM_BUILD_DEPRECATED /* LCOV_EXCL_START */
StanfordImporter::StanfordImporter() {
    configuration().setValue("perFaceToPerVertex", true);
    configuration().setValue("triangleFastPath", true);
    configuration().setValue("objectIdAttribute", "object_id");
}
#endif /* LCOV_EXCL_STOP */

StanfordImporter::StanfordImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

StanfordImporter::~StanfordImporter() = default;

ImporterFeatures StanfordImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool StanfordImporter::doIsOpened() const {
    /* Only one of these can be populated at a time; if Assimp is present it's
       also opened */
    CORRADE_INTERNAL_ASSERT(!_state || !_assimpImporter || _assimpImporter->isOpened());
    return _state || _assimpImporter;
}

void StanfordImporter::doClose() {
    _state = nullptr;
    _assimpImporter = nullptr;
}

namespace {

enum class PropertyType {
    Vertex = 1,
    Face
};

MeshIndexType parseIndexType(const Containers::StringView type) {
    if(type == "uchar"_s    || type == "uint8"_s ||
       type == "char"_s     || type == "int8"_s)
        return MeshIndexType::UnsignedByte;
    if(type == "ushort"_s   || type == "uint16"_s ||
       type == "short"_s    || type == "int16"_s)
        return MeshIndexType::UnsignedShort;
    if(type == "uint"_s     || type == "uint32"_s ||
       type == "int"_s      || type == "int32"_s)
        return MeshIndexType::UnsignedInt;

    return {};
}

VertexFormat parseAttributeType(const Containers::StringView type) {
    if(type == "uchar"_s    || type == "uint8"_s)
        return VertexFormat::UnsignedByte;
    if(type == "char"_s     || type == "int8"_s)
        return VertexFormat::Byte;
    if(type == "ushort"_s   || type == "uint16"_s)
        return VertexFormat::UnsignedShort;
    if(type == "short"_s    || type == "int16"_s)
        return VertexFormat::Short;
    if(type == "uint"_s     || type == "uint32"_s)
        return VertexFormat::UnsignedInt;
    if(type == "int"_s      || type == "int32"_s)
        return VertexFormat::Int;
    if(type == "float"_s    || type == "float32"_s)
        return VertexFormat::Float;
    if(type == "double"_s   || type == "float64"_s)
        return VertexFormat::Double;

    return {};
}

template<class T, class U> inline T extractValue(const char* buffer, const bool endianSwap) {
    /* do a memcpy() instead of *reinterpret_cast, as that'll correctly handle
       unaligned loads as well */
    U dest;
    std::memcpy(&dest, buffer, sizeof(U));
    if(endianSwap)
        Utility::Endianness::swapInPlace(dest);
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

/* Code of get_words() at https://paulbourke.net/dataformats/ply/plyfile.c
   checks for spaces and tabs, no other whitespace character. Assimp and
   Blender accept spaces and tabs as well. Besides that I'm considering a CR
   character. *Not* a newline, since that is a significant delimiter that has
   to be treated separately. */
constexpr Containers::StringView Whitespace = " \t\r"_s;

Containers::StringView extractLine(Containers::StringView& in) {
    const Containers::StringView lineEnd = in.findOr('\n', in.end());
    /* Code of get_words() at https://paulbourke.net/dataformats/ply/plyfile.c
       allows any amount of tabs and spaces at both start and end of a line.
       Assimp also allows spaces both at line start and end, so trim both.
       Blender (4.5.3) crashes with leading spaces. */
    const Containers::StringView line = in.prefix(lineEnd.begin()).trimmed(Whitespace);
    in = in.suffix(lineEnd.end());
    return line;
}

Containers::StringView extractToken(Containers::StringView& line) {
    /** @todo have some partitionOnAny() for this, or make findAny() consume
        all such characters, not just the first */
    const Containers::StringView tokenEnd = line.findAnyOr(Whitespace, line.end());
    const Containers::StringView token = line.prefix(tokenEnd.begin());
    /* Code of get_words() at https://paulbourke.net/dataformats/ply/plyfile.c
       allows any amount of tabs and spaces between tokens and both Assimp and
       Blender also allow multiple spaces between tokens so trim any additional
       space after the found one. The only exception is format line parsing in
       Assimp, where it expects exactly one space between the tokens, otherwise
       it rejects the file as invalid. Blender understands format lines with
       multiple spaces. */
    line = line.suffix(tokenEnd.end()).trimmedPrefix(Whitespace);
    return token;
}

template<std::size_t size> bool checkVectorAttributeValidity(const Math::Vector<size, VertexFormat>& formats, const Math::Vector<size, UnsignedInt>& offsets, const char* name) {
    /* Check that we have the same type for all position coordinates */
    if(formats != Math::Vector<size, VertexFormat>{formats[0]}) {
        Error{} << "Trade::StanfordImporter::openData(): expecting all" << name << "components to be present and have the same type but got" << formats;
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

void StanfordImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.isEmpty()) {
        Error{} << "Trade::StanfordImporter::openData(): the file is empty";
        return;
    }

    /* Turn the input into a string view so we can conveniently parse it */
    /** @todo check size < 1G on 32b? currently it'd just assert, but it's
        unlikely that such amount of contiguous memory would even be available
        there, so ¯\_(ツ)_/¯ */
    Containers::StringView in = data;

    /* Check file signature */
    {
        const Containers::StringView header = extractLine(in);
        if(header != "ply"_s) {
            Error{} << "Trade::StanfordImporter::openData(): invalid file signature" << header;
            return;
        }
    }

    /* Parse a `format` line. It can be preceded by any number of `comment` and
       empty lines. */
    Containers::Optional<bool> fileFormatNeedsEndianSwapping;
    while(in) {
        /* The (immutable) line is used for error messages only, parsing is
           done on lineIn */
        const Containers::StringView line = extractLine(in);
        Containers::StringView lineIn = line;
        const Containers::StringView keyword = extractToken(lineIn);

        /* Skip empty lines and comments */
        if(!keyword || keyword == "comment"_s)
            continue;

        /* After empty lines / comments there should be a format line */
        if(keyword != "format"_s) {
            Error{} << "Trade::StanfordImporter::openData(): expected format line, got" << line;
            return;
        }

        /* The format line should have a format and version, and nothing else
           after */
        const Containers::StringView format = extractToken(lineIn);
        const Containers::StringView version = extractToken(lineIn);
        /* Fails also if there are excess tokens at line end */
        if(!format || !version || lineIn) {
            Error() << "Trade::StanfordImporter::openData(): invalid format line" << line;
            return;
        }

        if(version == "1.0"_s) {
            if(format == "binary_little_endian"_s) {
                fileFormatNeedsEndianSwapping = Utility::Endianness::isBigEndian();
                break;
            } else if(format == "binary_big_endian"_s) {
                fileFormatNeedsEndianSwapping = !Utility::Endianness::isBigEndian();
                break;
            } else if(format == "ascii"_s) {
                constexpr Containers::StringView plugin = "AssimpImporter"_s;
                if(
                    #ifdef MAGNUM_BUILD_DEPRECATED
                    /** @todo remove the !manager() once deprecated
                        manager-less instantiation is removed */
                    !manager() ||
                    #endif
                    !(manager()->load(plugin) & PluginManager::LoadState::Loaded))
                {
                    Error{} << "Trade::StanfordImporter::openData(): can't forward an ASCII file to AssimpImporter";
                    return;
                }

                if(flags() & ImporterFlag::Verbose)
                    Debug{} << "Trade::StanfordImporter::openData(): forwarding an ASCII file to AssimpImporter";

                /* Instantiate the plugin, propagate flags. PLYs can't
                   reference external data so file callbacks don't need to be
                   propagated. */
                Containers::Pointer<AbstractImporter> assimpImporter = static_cast<PluginManager::Manager<AbstractImporter>*>(manager())->instantiate(plugin);
                assimpImporter->setFlags(flags());

                /* Try to open the data with AssimpImporter (error output
                   should be printed by the plugin itself). All other functions
                   transparently forward to that importer instance if it's
                   populated. */
                if(!assimpImporter->openData(data))
                    return;

                /* Success, save the instance */
                _assimpImporter = Utility::move(assimpImporter);
                return;
            }
        }

        Error{} << "Trade::StanfordImporter::openData(): unsupported file format" << format << version;
        return;
    }

    /* Header checks passed and we're not delegating to Assimp, take over the
       existing array or copy the data if we can't. If copying, re-routed the
       input view to the copied array. */
    Containers::Array<char> dataCopy;
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned))
        dataCopy = Utility::move(data);
    else {
        dataCopy = Containers::Array<char>{InPlaceInit, data};
        in = dataCopy.exceptPrefix(in.begin() - data.begin());
    }

    /* Initialize the importer state */
    Containers::Pointer<State> state{InPlaceInit};

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
    /* The property type affects parsing of the following lines so it has to be
       outside of the per-line loop */
    PropertyType propertyType{};
    while(in) {
        /* The (immutable) line is used for error messages only, parsing is
           done on lineIn */
        const Containers::StringView line = extractLine(in);
        Containers::StringView lineIn = line;
        const Containers::StringView keyword = extractToken(lineIn);

        /* Skip empty lines and comments */
        if(!keyword || keyword == "comment"_s)
            continue;

        /* Elements */
        if(keyword == "element"_s) {
            const Containers::StringView type = extractToken(lineIn);
            const Containers::StringView count = extractToken(lineIn);

            /* Vertex or face elements. Handled together so we don't need to
               have a helper for integer parsing. */
            if((type == "vertex"_s || type == "face"_s) && count && !lineIn) {
                /* strtoull() doesn't take a size argument, which means it runs
                   off until it finds something that doesn't look like a
                   numeric character. It can however happen that the number is
                   at the end of a file, at which point it'd attempt to read
                   garbage memory. There has to be an end_header line after at
                   some point, so if the line ends at the end of the file, we
                   can directly treat that as an error. */
                if(line.end() == dataCopy.end()) {
                    Error{} << "Trade::StanfordImporter::openData(): incomplete header";
                    return;
                }

                char* end;
                /* Not using strtol() here as on Windows it's 32-bit and we'd
                   have to additionally check errno to detect overflows */
                const UnsignedLong outLong = std::strtoull(count.data(), &end, 10);
                if(!count || end != count.end()) {
                    Error{} << "Trade::StanfordImporter::openData(): invalid integer literal" << count;
                    return;
                }
                if(outLong > ~UnsignedInt{}) {
                    Error{} << "Trade::StanfordImporter::openData(): too large integer literal" << count;
                    return;
                }

                if(type == "vertex"_s) {
                    state->vertexCount = outLong;
                    propertyType = PropertyType::Vertex;
                } else if(type == "face"_s) {
                    state->faceCount = outLong;
                    propertyType = PropertyType::Face;
                } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

            /* Something else, or excess tokens at line end */
            } else {
                Error{} << "Trade::StanfordImporter::openData(): invalid element line" << line;
                return;
            }

        /* Element properties */
        } else if(keyword == "property"_s) {
            /* Vertex element properties */
            if(propertyType == PropertyType::Vertex) {
                const Containers::StringView type = extractToken(lineIn);
                const Containers::StringView component = extractToken(lineIn);
                /* Fails also if there are excess tokens at line end */
                if(!type || !component || lineIn) {
                    Error{} << "Trade::StanfordImporter::openData(): invalid vertex property line" << line;
                    return;
                }

                /* Component type */
                const VertexFormat componentFormat = parseAttributeType(type);
                if(componentFormat == VertexFormat{}) {
                    Error{} << "Trade::StanfordImporter::openData(): invalid vertex component type" << type;
                    return;
                }

                /* Component */
                if(component == "x"_s) {
                    positionOffsets.x() = state->vertexStride;
                    positionFormats.x() = componentFormat;
                } else if(component == "y"_s) {
                    positionOffsets.y() = state->vertexStride;
                    positionFormats.y() = componentFormat;
                } else if(component == "z"_s) {
                    positionOffsets.z() = state->vertexStride;
                    positionFormats.z() = componentFormat;
                } else if(component == "nx"_s) {
                    normalOffsets.x() = state->vertexStride;
                    normalFormats.x() = componentFormat;
                } else if(component == "ny"_s) {
                    normalOffsets.y() = state->vertexStride;
                    normalFormats.y() = componentFormat;
                } else if(component == "nz"_s) {
                    normalOffsets.z() = state->vertexStride;
                    normalFormats.z() = componentFormat;
                /* LuxBlend uses s/t, Mitsuba uses u/v */
                } else if(component == "u"_s || component == "s"_s) {
                    textureCoordinateOffsets.x() = state->vertexStride;
                    textureCoordinateFormats.x() = componentFormat;
                } else if(component == "v"_s || component == "t"_s) {
                    textureCoordinateOffsets.y() = state->vertexStride;
                    textureCoordinateFormats.y() = componentFormat;
                } else if(component == "red"_s) {
                    colorOffsets.x() = state->vertexStride;
                    colorFormats.x() = componentFormat;
                } else if(component == "green"_s) {
                    colorOffsets.y() = state->vertexStride;
                    colorFormats.y() = componentFormat;
                } else if(component == "blue"_s) {
                    colorOffsets.z() = state->vertexStride;
                    colorFormats.z() = componentFormat;
                /* Several people complain that Meshlab doesn't support alpha,
                   so let's make sure we do :P
                    https://github.com/cnr-isti-vclab/meshlab/issues/161 */
                } else if(component == "alpha"_s) {
                    colorOffsets.w() = state->vertexStride;
                    colorFormats.w() = componentFormat;
                } else if(component == configuration().value<Containers::StringView>("objectIdAttribute")) {
                    objectIdOffset = state->vertexStride;
                    objectIdFormat = componentFormat;

                /* Unknown component, add to the attribute list. Stride is not
                   known yet, using 0 until it's updated later. */
                } else {
                    auto inserted = state->attributeNameMap.emplace(component,
                        meshAttributeCustom(state->attributeNames.size()));
                    arrayAppend(state->attributeNames, component);
                    arrayAppend(state->attributeData, MeshAttributeData{
                        inserted.first->second,
                        componentFormat,
                        state->vertexStride, state->vertexCount, 0});
                }

                /* Add size of current component to total stride */
                state->vertexStride += vertexFormatSize(componentFormat);

            /* Face element properties */
            } else if(propertyType == PropertyType::Face) {
                const Containers::StringView type = extractToken(lineIn);

                /* Face vertex indices. The vertex_indices name is usual,
                   Assimp exports with vertex_index, reference from
                    https://paulbourke.net/dataformats/ply/ mentions both. */
                if(type == "list"_s) {
                    const Containers::StringView sizeType = extractToken(lineIn);
                    const Containers::StringView indexType = extractToken(lineIn);
                    const Containers::StringView name = extractToken(lineIn);
                    /* Fails also if there are excess tokens at line end */
                    if((name != "vertex_indices"_s && name != "vertex_index"_s) || lineIn) {
                        Error{} << "Trade::StanfordImporter::openData(): invalid face index list property line" << line;
                        return;
                    }

                    state->faceIndicesOffset = state->faceSkip;
                    state->faceSkip = 0;

                    /* Face size type */
                    if((state->faceSizeType = parseIndexType(sizeType)) == MeshIndexType{}) {
                        Error{} << "Trade::StanfordImporter::openData(): invalid face size type" << sizeType;
                        return;
                    }

                    /* Face index type */
                    if((state->faceIndexType = parseIndexType(indexType)) == MeshIndexType{}) {
                        Error{} << "Trade::StanfordImporter::openData(): invalid face index type" << indexType;
                        return;
                    }

                /* Per-face component */
                } else {
                    const VertexFormat componentFormat = parseAttributeType(type);
                    const Containers::StringView component = extractToken(lineIn);
                    /* Fails also if there are excess tokens at line end */
                    if(!component || lineIn) {
                        Error{} << "Trade::StanfordImporter::openData(): invalid face property line" << line;
                        return;
                    }

                    if(componentFormat == VertexFormat{}) {
                        Error{} << "Trade::StanfordImporter::openData(): invalid face component type" << type;
                        return;
                    }

                    /* Before indices are found, faceIndicesOffset is zero.
                       After indices are found, faceIndicesOffset is set and
                       faceSkip is zero again, thus the sum of the two is
                       always offset from the beginning of the face, which is
                       what we need. */
                    const UnsignedInt faceComponentOffset =
                        state->faceIndicesOffset + state->faceSkip;

                    /* Per-face normals and colors make sense, OTOH positions
                       or texture coordinates don't, so not handling those in
                       any way (they would appear as custom attributes) */
                    if(component == "nx"_s) {
                        perFaceNormals = true;
                        normalOffsets.x() = faceComponentOffset;
                        normalFormats.x() = componentFormat;
                    } else if(component == "ny"_s) {
                        normalOffsets.y() = faceComponentOffset;
                        normalFormats.y() = componentFormat;
                    } else if(component == "nz"_s) {
                        normalOffsets.z() = faceComponentOffset;
                        normalFormats.z() = componentFormat;
                    } else if(component == "red"_s) {
                        perFaceColors = true;
                        colorOffsets.x() = faceComponentOffset;
                        colorFormats.x() = componentFormat;
                    } else if(component == "green"_s) {
                        colorOffsets.y() = faceComponentOffset;
                        colorFormats.y() = componentFormat;
                    } else if(component == "blue"_s) {
                        colorOffsets.z() = faceComponentOffset;
                        colorFormats.z() = componentFormat;
                    } else if(component == "alpha"_s) {
                        colorOffsets.w() = faceComponentOffset;
                        colorFormats.w() = componentFormat;
                    } else if(component == configuration().value<Containers::StringView>("objectIdAttribute")) {
                        perFaceObjectIds = true;
                        objectIdOffset = faceComponentOffset;
                        objectIdFormat = componentFormat;

                    /* Unknown component, add to the face attribute list.
                       Stride and actual triangle face count is not known yet,
                       using 0 until it's updated later. */
                    } else {
                        auto inserted = state->attributeNameMap.emplace(component,
                            meshAttributeCustom(state->attributeNames.size()));
                        arrayAppend(state->attributeNames, component);
                        arrayAppend(state->faceAttributeData, MeshAttributeData{
                            inserted.first->second,
                            componentFormat, faceComponentOffset, 0, 0});
                    }

                    state->faceSkip += vertexFormatSize(componentFormat);
                }

            /* Unexpected property line */
            } else if(propertyType == PropertyType{}) {
                Error{} << "Trade::StanfordImporter::openData(): unexpected property line";
                return;
            }

        /* Header end */
        } else if(keyword == "end_header"_s) {
            break;

        /* Something else */
        } else {
            Error{} << "Trade::StanfordImporter::openData(): unknown line" << line;
            return;
        }
    }

    /* Check header consistency */
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
        /* Check that positions are there at all and that all components have
           the same type and are right after each other */
        if((positionOffsets >= Vector3ui{~UnsignedInt{}}).all()) {
            Error{} << "Trade::StanfordImporter::openData(): no position components present";
            return;
        }
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
    state->data = Utility::move(dataCopy);
    state->headerSize = state->data.size() - in.size();
    _state = Utility::move(state);
}

UnsignedInt StanfordImporter::doMeshCount() const {
    if(_assimpImporter)
        return _assimpImporter->meshCount();

    return 1;
}

UnsignedInt StanfordImporter::doMeshLevelCount(UnsignedInt id) {
    if(_assimpImporter)
        return _assimpImporter->meshLevelCount(id);

    return configuration().value<bool>("perFaceToPerVertex") ? 1 : 2;
}

Containers::Optional<MeshData> StanfordImporter::doMesh(UnsignedInt id, const UnsignedInt level) {
    if(_assimpImporter)
        return _assimpImporter->mesh(id, level);

    /* We either have per-face in the second level or we convert them to
       per-vertex, never both */
    CORRADE_INTERNAL_ASSERT(!(level == 1 && configuration().value<bool>("perFaceToPerVertex")));
    const bool parsePerFaceAttributes = level == 1 ||
        configuration().value<bool>("perFaceToPerVertex");

    Containers::ArrayView<const char> in = _state->data.exceptPrefix(_state->headerSize);

    /* Copy all vertex data */
    Containers::Array<char> vertexData;
    if(level == 0)
        vertexData = Containers::Array<char>{InPlaceInit, in.prefix(_state->vertexStride*_state->vertexCount)};
    in = in.exceptPrefix(_state->vertexStride*_state->vertexCount);

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
                src.exceptPrefix({0, _state->faceIndicesOffset + faceSizeTypeSize + 3*faceIndexTypeSize}),
                dst.exceptPrefix({0, _state->faceIndicesOffset}));
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
            in = in.exceptPrefix(_state->faceIndicesOffset);

            /* Get face size */
            const Containers::ArrayView<const char> faceSizeData = in.prefix(faceSizeTypeSize);
            in = in.exceptPrefix(faceSizeTypeSize);
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
            in = in.exceptPrefix(faceIndexTypeSize*faceSize);
            const Containers::ArrayView<const char> faceDataAfterIndices = in.prefix(_state->faceSkip);
            in = in.exceptPrefix(_state->faceSkip);

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
                if(formatSize == 1)
                    continue;
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
    if(level == 0 && configuration().value<bool>("perFaceToPerVertex") && !faceAttributeData.isEmpty()) {
        if(flags() & ImporterFlag::Verbose)
            Debug{} << "Trade::StanfordImporter::mesh(): converting" << faceAttributeData.size() << "per-face attributes to per-vertex";

        /** @todo in this case it'll assert if indices are out of range, check
            for it at runtime somehow */
        MeshIndexData indices{_state->faceIndexType, indexData};
        MeshData perVertex{MeshPrimitive::Triangles,
            Utility::move(indexData), indices,
            Utility::move(vertexData), Utility::move(vertexAttributeData)};
        MeshData perFace{MeshPrimitive::Faces,
            Utility::move(faceData), Utility::move(faceAttributeData), triangleFaceCount};
        return MeshTools::combineFaceAttributes(perVertex, perFace);
    }

    if(level == 0) {
        MeshIndexData indices{_state->faceIndexType, indexData};
        return MeshData{MeshPrimitive::Triangles,
            Utility::move(indexData), indices,
            Utility::move(vertexData), Utility::move(vertexAttributeData)};
    } else {
        return MeshData{MeshPrimitive::Faces,
            Utility::move(faceData), Utility::move(faceAttributeData), triangleFaceCount};
    }
}

MeshAttribute StanfordImporter::doMeshAttributeForName(const Containers::StringView name) {
    if(_assimpImporter)
        return _assimpImporter->meshAttributeForName(name);

    return _state ? _state->attributeNameMap[name] : MeshAttribute{};
}

Containers::String StanfordImporter::doMeshAttributeName(MeshAttribute name) {
    if(_assimpImporter)
        return _assimpImporter->meshAttributeName(name);

    return _state && meshAttributeCustom(name) < _state->attributeNames.size() ?
        _state->attributeNames[meshAttributeCustom(name)] : "";
}

}}

CORRADE_PLUGIN_REGISTER(StanfordImporter, Magnum::Trade::StanfordImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
