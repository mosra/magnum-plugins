/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>

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

#include <fstream>
#include <sstream>
#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/String.h>
#include <Corrade/Utility/Endianness.h>
#include <Magnum/Array.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/MeshData3D.h>

namespace Magnum { namespace Trade {

StanfordImporter::StanfordImporter() = default;

StanfordImporter::StanfordImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

StanfordImporter::~StanfordImporter() = default;

auto StanfordImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool StanfordImporter::doIsOpened() const { return !!_in; }

void StanfordImporter::doClose() { _in = nullptr; }

void StanfordImporter::doOpenFile(const std::string& filename) {
    /* Open file in *binary* mode to avoid broken binary data (need to handle \r manually) */
    std::unique_ptr<std::istream> in{new std::ifstream{filename, std::ifstream::binary}};
    if(!in->good()) {
        Error() << "Trade::ObjImporter::openFile(): cannot open file" << filename;
        return;
    }

    _in = std::move(in);
}

void StanfordImporter::doOpenData(const Containers::ArrayView<const char> data) {
    _in.reset(new std::istringstream{{data, data.size()}});
}

UnsignedInt StanfordImporter::doMesh3DCount() const { return 1; }

namespace {

enum class FileFormat {
    LittleEndian = 1,
    BigEndian = 2
};

enum class Type {
    UnsignedByte = 1,
    Byte,
    UnsignedShort,
    Short,
    UnsignedInt,
    Int,
    Float,
    Double
};

enum class PropertyType {
    Vertex = 1,
    Face,
    Ignored
};

Type parseType(const std::string& type) {
    if(type == "uchar"  || type == "uint8")     return Type::UnsignedByte;
    if(type == "char"   || type == "int8")      return Type::Byte;
    if(type == "ushort" || type == "uint16")    return Type::UnsignedShort;
    if(type == "short"  || type == "int16")     return Type::Short;
    if(type == "uint"   || type == "uint32")    return Type::UnsignedInt;
    if(type == "int"    || type == "int32")     return Type::Int;
    if(type == "float"  || type == "float32")   return Type::Float;
    if(type == "double" || type == "float64")   return Type::Double;

    return {};
}

std::size_t sizeOf(Type type) {
    switch(type) {
        #define _c(type) case Type::type: return sizeof(type);
        _c(UnsignedByte)
        _c(Byte)
        _c(UnsignedShort)
        _c(Short)
        _c(UnsignedInt)
        _c(Int)
        _c(Float)
        _c(Double)
        #undef _c
    }

    CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

template<FileFormat format, class T> struct EndianSwap;
template<class T> struct EndianSwap<FileFormat::LittleEndian, T> {
    constexpr T operator()(T value) const { return Utility::Endianness::littleEndian<T>(value); }
};
template<class T> struct EndianSwap<FileFormat::BigEndian, T> {
    constexpr T operator()(T value) const { return Utility::Endianness::bigEndian<T>(value); }
};

template<class T, FileFormat format, class U> inline T extractAndSkip(const char*& buffer) {
    const auto result = T(EndianSwap<format, U>{}(*reinterpret_cast<const U*>(buffer)));
    buffer += sizeof(U);
    return result;
}

template<class T, FileFormat format> T extractAndSkip(const char*& buffer, const Type type) {
    switch(type) {
        #define _c(type) case Type::type: return extractAndSkip<T, format, type>(buffer);
        _c(UnsignedByte)
        _c(Byte)
        _c(UnsignedShort)
        _c(Short)
        _c(UnsignedInt)
        _c(Int)
        _c(Float)
        _c(Double)
        #undef _c
    }

    CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

template<class T> inline T extractAndSkip(const char*& buffer, const FileFormat fileFormat, const Type type) {
    return fileFormat == FileFormat::LittleEndian ?
        extractAndSkip<T, FileFormat::LittleEndian>(buffer, type) :
        extractAndSkip<T, FileFormat::BigEndian>(buffer, type);
}

template<class T> inline T extract(const char* const buffer, const FileFormat fileFormat, const Type type) {
    const char* mutableBuffer = buffer;
    return extractAndSkip<T>(mutableBuffer, fileFormat, type);
}

inline void extractTriangle(std::vector<UnsignedInt>& indices, const char* const buffer, const FileFormat fileFormat, const Type indexType) {
    const char* position = buffer;

    indices.insert(indices.end(), {
        extractAndSkip<UnsignedInt>(position, fileFormat, indexType),
        extractAndSkip<UnsignedInt>(position, fileFormat, indexType),
        extractAndSkip<UnsignedInt>(position, fileFormat, indexType)
    });
}

inline void extractQuad(std::vector<UnsignedInt>& indices, const char* const buffer, const FileFormat fileFormat, const Type indexType) {
    const char* position = buffer;

    /* GCC <=4.8 doesn't properly sequence the operations in list-initializer
       (e.g. Vector4ui{extractAndSkip(), extractAndSkip(), ...}, so I need to
       make the order really explicit. From what I understood from the specs,
       this should be defined when using {}. Am I right? */
    Vector4ui quad{Math::NoInit};
    quad[0] = extractAndSkip<UnsignedInt>(position, fileFormat, indexType);
    quad[1] = extractAndSkip<UnsignedInt>(position, fileFormat, indexType);
    quad[2] = extractAndSkip<UnsignedInt>(position, fileFormat, indexType);
    quad[3] = extractAndSkip<UnsignedInt>(position, fileFormat, indexType);

    /* 0 0---3
       |\ \  |
       | \ \ |
       |  \ \|
       1---2 2 */
    indices.insert(indices.end(), {
        quad[0],
        quad[1],
        quad[2],
        quad[0],
        quad[2],
        quad[3]
    });
}

}

std::optional<MeshData3D> StanfordImporter::doMesh3D(UnsignedInt) {
    _in->seekg(0);

    /* Check file signature */
    {
        std::string header;
        std::getline(*_in, header);
        header = Utility::String::rtrim(std::move(header));
        if(header != "ply") {
            Error() << "Trade::StanfordImporter::mesh3D(): invalid file signature" << header;
            return std::nullopt;
        }
    }

    /* Parse format line */
    FileFormat fileFormat{};
    {
        std::string line;
        while(std::getline(*_in, line)) {
            std::vector<std::string> tokens = Utility::String::splitWithoutEmptyParts(line);

            /* Skip empty lines and comments */
            if(tokens.empty() || tokens.front() == "comment")
                continue;

            if(tokens[0] != "format") {
                Error() << "Trade::StanfordImporter::mesh3D(): expected format line";
                return std::nullopt;
            }

            if(tokens.size() != 3) {
                Error() << "Trade::StanfordImporter::mesh3D(): invalid format line" << line;
                return std::nullopt;
            }

            if(tokens[2] == "1.0") {
                if(tokens[1] == "binary_little_endian") {
                    fileFormat = FileFormat::LittleEndian;
                    break;
                } else if(tokens[1] == "binary_big_endian") {
                    fileFormat = FileFormat::BigEndian;
                    break;
                }
            }

            Error() << "Trade::StanfordImporter::mesh3D(): unsupported file format" << tokens[1] << tokens[2];
            return std::nullopt;
        }
    }

    /* Check format line consistency */
    if(fileFormat == FileFormat{}) {
        Error() << "Trade::StanfordImporter::mesh3D(): missing format line";
        return std::nullopt;
    }

    /* Parse rest of the header */
    UnsignedInt stride{}, vertexCount{}, faceCount{};
    Array3D<Type> componentTypes;
    Type faceSizeType{}, faceIndexType{};
    Vector3i componentOffsets{-1};
    {
        std::size_t componentOffset = 0;
        std::string line;
        PropertyType propertyType{};
        while(std::getline(*_in, line)) {
            std::vector<std::string> tokens = Utility::String::splitWithoutEmptyParts(line);

            /* Skip empty lines and comments */
            if(tokens.empty() || tokens.front() == "comment")
                continue;

            /* Elements */
            if(tokens[0] == "element") {
                /* Vertex elements */
                if(tokens.size() == 3 && tokens[1] == "vertex") {
                    #ifndef CORRADE_TARGET_ANDROID
                    vertexCount = std::stoi(tokens[2]);
                    #else
                    vertexCount = std::strtoul(tokens[2].data(), nullptr, 10);
                    #endif
                    propertyType = PropertyType::Vertex;

                /* Face elements */
                } else if(tokens.size() == 3 &&tokens[1] == "face") {
                    #ifndef CORRADE_TARGET_ANDROID
                    faceCount = std::stoi(tokens[2]);
                    #else
                    faceCount = std::strtoul(tokens[2].data(), nullptr, 10);
                    #endif
                    propertyType = PropertyType::Face;

                /* Something else */
                } else {
                    Error() << "Trade::StanfordImporter::mesh3D(): unknown element" << tokens[1];
                    return std::nullopt;
                }

            /* Element properties */
            } else if(tokens[0] == "property") {
                /* Vertex element properties */
                if(propertyType == PropertyType::Vertex) {
                    if(tokens.size() != 3) {
                        Error() << "Trade::StanfordImporter::mesh3D(): invalid vertex property line" << line;
                        return std::nullopt;
                    }

                    /* Component type */
                    const Type componentType = parseType(tokens[1]);
                    if(componentType == Type{}) {
                        Error() << "Trade::StanfordImporter::mesh3D(): invalid vertex component type" << tokens[1];
                        return std::nullopt;
                    }

                    /* Component */
                    if(tokens[2] == "x") {
                        componentOffsets.x() = componentOffset;
                        componentTypes.x() = componentType;
                    } else if(tokens[2] == "y") {
                        componentOffsets.y() = componentOffset;
                        componentTypes.y() = componentType;
                    } else if(tokens[2] == "z") {
                        componentOffsets.z() = componentOffset;
                        componentTypes.z() = componentType;
                    }
                    else Debug() << "Trade::StanfordImporter::mesh3D(): ignoring unknown vertex component" << tokens[2];

                    /* Add size of current component to total offset */
                    componentOffset += sizeOf(componentType);

                /* Face element properties */
                } else if(propertyType == PropertyType::Face) {
                    if(tokens.size() != 5 || tokens[1] != "list" || tokens[4] != "vertex_indices") {
                        Error() << "Trade::StanfordImporter::mesh3D(): unknown face property line" << line;
                        return std::nullopt;
                    }

                    /* Face size type */
                    if((faceSizeType = parseType(tokens[2])) == Type{}) {
                        Error() << "Trade::StanfordImporter::mesh3D(): invalid face size type" << tokens[2];
                        return std::nullopt;
                    }

                    /* Face index type */
                    if((faceIndexType = parseType(tokens[3])) == Type{}) {
                        Error() << "Trade::StanfordImporter::mesh3D(): invalid face index type" << tokens[3];
                        return std::nullopt;
                    }

                /* Unexpected property line */
                } else if(propertyType != PropertyType::Ignored) {
                    Error() << "Trade::StanfordImporter::mesh3D(): unexpected property line";
                    return std::nullopt;
                }

            /* Header end */
            } else if(tokens[0] == "end_header") {
                break;

            /* Something else */
            } else {
                Error() << "Trade::StanfordImporter::mesh3D(): unknown line" << line;
                return std::nullopt;
            }
        }

        stride = componentOffset;
    }

    /* Check header consistency */
    if((componentOffsets < Vector3i{0}).any()) {
        Error() << "Trade::StanfordImporter::mesh3D(): incomplete vertex specification";
        return std::nullopt;
    }
    if(faceSizeType == Type{} || faceIndexType == Type{}) {
        Error() << "Trade::StanfordImporter::mesh3D(): incomplete face specification";
        return std::nullopt;
    }

    /* Parse vertices */
    std::vector<Vector3> positions;
    positions.reserve(vertexCount);
    {
        Containers::Array<char> buffer{stride};
        for(std::size_t i = 0; i != vertexCount; ++i) {
            _in->read(buffer, stride);
            positions.emplace_back(
                extract<Float>(buffer + componentOffsets.x(), fileFormat, componentTypes.x()),
                extract<Float>(buffer + componentOffsets.y(), fileFormat, componentTypes.y()),
                extract<Float>(buffer + componentOffsets.z(), fileFormat, componentTypes.z())
            );
        }
    }

    /* Parse faces, reserve optimistically amount for all-triangle faces */
    std::vector<UnsignedInt> indices;
    indices.reserve(faceCount*3);
    {
        /* Assuming max four four-byte indices per face */
        char buffer[4*4];

        const UnsignedInt faceSizeTypeSize = sizeOf(faceSizeType);
        const UnsignedInt faceIndexTypeSize = sizeOf(faceIndexType);
        for(std::size_t i = 0; i != faceCount; ++i) {
            /* Get face size */
            _in->read(buffer, faceSizeTypeSize);
            const UnsignedInt faceSize = extract<UnsignedInt>(buffer, fileFormat, faceSizeType);
            if(faceSize < 3 || faceSize > 4) {
                Error() << "Trade::StanfordImporter::mesh3D(): unsupported face size" << faceSize;
                return std::nullopt;
            }

            /* Parse face indices */
            if(!_in->read(buffer, faceIndexTypeSize*faceSize)) {
                Error() << "Trade::StanfordImporter::mesh3D(): file is too short";
                return std::nullopt;
            }
            faceSize == 3 ?
                extractTriangle(indices, buffer, fileFormat, faceIndexType) :
                extractQuad(indices, buffer, fileFormat, faceIndexType);
        }
    }

    return MeshData3D{MeshPrimitive::Triangles, std::move(indices), {std::move(positions)}, {}, {}, {}, nullptr};
}

}}
