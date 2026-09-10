/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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

#include "JtImporter.h"

#include <Corrade/Containers/EnumSet.hpp>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Packing.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Trade/MeshData.h>
#include <lzma.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

struct JtImporter::State {
};

JtImporter::JtImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

JtImporter::~JtImporter() = default;

ImporterFeatures JtImporter::doFeatures() const {
    return ImporterFeature::OpenData;
}

bool JtImporter::doIsOpened() const {
    return !!_state;
}

void JtImporter::doClose() {
    _state = {};
}

namespace {

struct Guid {
    UnsignedInt a;
    UnsignedShort b, c;
    UnsignedByte d, e, f, g, h, i, j, k;

    bool operator==(const Guid& other) const {
        return other.a == a &&
               other.b == b &&
               other.c == c &&
               other.d == d &&
               other.e == e &&
               other.f == f &&
               other.g == g &&
               other.h == h &&
               other.i == i &&
               other.j == j &&
               other.k == k;
    }
    bool operator!=(const Guid& other) const {
        return !operator==(other);
    }
};

Debug& operator<<(Debug& out, const Guid& value) {
    char data[44];
    Utility::formatInto(data,
        "{{{:.8x}-{:.4x}-{:.4x}-{:.2x}-{:.2x}-{:.2x}-{:.2x}-{:.2x}-{:.2x}-{:.2x}-{:.2x}}}",
        value.a,
        value.b,
        value.c,
        value.d,
        value.e,
        value.f,
        value.g,
        value.h,
        value.i,
        value.j,
        value.k);
    return out << Containers::StringView{data, sizeof(data)};
}

template<class T> T read(const char& data) {
    // TODO uh actually unaligned read
    return reinterpret_cast<const T&>(data);
}

/* Ordered the same as the table in the 10.6 spec PDF */
constexpr Guid InstanceNodeElementId{0x10dd102a, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
constexpr Guid PartitionNodeElementId{0x10dd103e, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
constexpr Guid GeometricTransformAttributeElementId{0x10dd1083, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
constexpr Guid MaterialAttributeElementId{0x10dd1030, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
constexpr Guid TriStripSetShapeNodeElementId{0x10dd1077, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
constexpr Guid DatePropertyAtomElementId{0xce357246, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1};
constexpr Guid IntegerPropertyAtomElementId{0x10dd102b, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
constexpr Guid FloatingPointPropertyAtomElementId{0x10dd1019, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
constexpr Guid LateLoadedPropertyAtomElementId{0xe0b05be5, 0xfbbd, 0x11d1, 0xa3, 0xa7, 0x00, 0xaa, 0x00, 0xd1, 0x09, 0x54};
constexpr Guid StringPropertyAtomElementId{0x10dd106e, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};

enum class VertexBinding: UnsignedLong {
    Position2D = 1ull << 0,
    Position3D = 1ull << 1,
    Position4D = 1ull << 2,
    Normal = 1ull << 3,
    Color3 = 1ull << 4,
    Color4 = 1ull << 5,
    Flag = 1ull << 6,
    TextureCoordinates01D = 1ull << 8,
    TextureCoordinates02D = 1ull << 9,
    TextureCoordinates03D = 1ull << 10,
    TextureCoordinates04D = 1ull << 11,
    TextureCoordinates11D = 1ull << 12,
    TextureCoordinates12D = 1ull << 13,
    TextureCoordinates13D = 1ull << 14,
    TextureCoordinates14D = 1ull << 15,
    TextureCoordinates21D = 1ull << 16,
    TextureCoordinates22D = 1ull << 17,
    TextureCoordinates23D = 1ull << 18,
    TextureCoordinates24D = 1ull << 19,
    TextureCoordinates31D = 1ull << 20,
    TextureCoordinates32D = 1ull << 21,
    TextureCoordinates33D = 1ull << 22,
    TextureCoordinates34D = 1ull << 23,
    TextureCoordinates41D = 1ull << 24,
    TextureCoordinates42D = 1ull << 25,
    TextureCoordinates43D = 1ull << 26,
    TextureCoordinates44D = 1ull << 27,
    TextureCoordinates51D = 1ull << 28,
    TextureCoordinates52D = 1ull << 29,
    TextureCoordinates53D = 1ull << 30,
    TextureCoordinates54D = 1ull << 31,
    TextureCoordinates61D = 1ull << 32,
    TextureCoordinates62D = 1ull << 33,
    TextureCoordinates63D = 1ull << 34,
    TextureCoordinates64D = 1ull << 35,
    TextureCoordinates71D = 1ull << 36,
    TextureCoordinates72D = 1ull << 37,
    TextureCoordinates73D = 1ull << 38,
    TextureCoordinates74D = 1ull << 39,
    Auxiliary = 1ull << 63
};
typedef Containers::EnumSet<VertexBinding> VertexBindings;
Debug& operator<<(Debug& debug, const VertexBinding value) {
    switch(value) {
        #define _c(value) case VertexBinding::value: return debug << #value;
        _c(Position2D)
        _c(Position3D)
        _c(Position4D)
        _c(Normal)
        _c(Color3)
        _c(Color4)
        _c(Flag)
        _c(TextureCoordinates01D)
        _c(TextureCoordinates02D)
        _c(TextureCoordinates03D)
        _c(TextureCoordinates04D)
        _c(TextureCoordinates11D)
        _c(TextureCoordinates12D)
        _c(TextureCoordinates13D)
        _c(TextureCoordinates14D)
        _c(TextureCoordinates21D)
        _c(TextureCoordinates22D)
        _c(TextureCoordinates23D)
        _c(TextureCoordinates24D)
        _c(TextureCoordinates31D)
        _c(TextureCoordinates32D)
        _c(TextureCoordinates33D)
        _c(TextureCoordinates34D)
        _c(TextureCoordinates41D)
        _c(TextureCoordinates42D)
        _c(TextureCoordinates43D)
        _c(TextureCoordinates44D)
        _c(TextureCoordinates51D)
        _c(TextureCoordinates52D)
        _c(TextureCoordinates53D)
        _c(TextureCoordinates54D)
        _c(TextureCoordinates61D)
        _c(TextureCoordinates62D)
        _c(TextureCoordinates63D)
        _c(TextureCoordinates64D)
        _c(TextureCoordinates71D)
        _c(TextureCoordinates72D)
        _c(TextureCoordinates73D)
        _c(TextureCoordinates74D)
        _c(Auxiliary)
        #undef _c
    }

    return debug << Debug::hex << UnsignedLong(value);
}
Debug& operator<<(Debug& debug, const VertexBindings value) {
    return Containers::enumSetDebugOutput(debug, value, "{}", {
        VertexBinding::Position2D,
        VertexBinding::Position3D,
        VertexBinding::Position4D,
        VertexBinding::Normal,
        VertexBinding::Color3,
        VertexBinding::Color4,
        VertexBinding::Flag,
        VertexBinding::TextureCoordinates01D,
        VertexBinding::TextureCoordinates02D,
        VertexBinding::TextureCoordinates03D,
        VertexBinding::TextureCoordinates04D,
        VertexBinding::TextureCoordinates11D,
        VertexBinding::TextureCoordinates12D,
        VertexBinding::TextureCoordinates13D,
        VertexBinding::TextureCoordinates14D,
        VertexBinding::TextureCoordinates21D,
        VertexBinding::TextureCoordinates22D,
        VertexBinding::TextureCoordinates23D,
        VertexBinding::TextureCoordinates24D,
        VertexBinding::TextureCoordinates31D,
        VertexBinding::TextureCoordinates32D,
        VertexBinding::TextureCoordinates33D,
        VertexBinding::TextureCoordinates34D,
        VertexBinding::TextureCoordinates41D,
        VertexBinding::TextureCoordinates42D,
        VertexBinding::TextureCoordinates43D,
        VertexBinding::TextureCoordinates44D,
        VertexBinding::TextureCoordinates51D,
        VertexBinding::TextureCoordinates52D,
        VertexBinding::TextureCoordinates53D,
        VertexBinding::TextureCoordinates54D,
        VertexBinding::TextureCoordinates61D,
        VertexBinding::TextureCoordinates62D,
        VertexBinding::TextureCoordinates63D,
        VertexBinding::TextureCoordinates64D,
        VertexBinding::TextureCoordinates71D,
        VertexBinding::TextureCoordinates72D,
        VertexBinding::TextureCoordinates73D,
        VertexBinding::TextureCoordinates74D,
        VertexBinding::Auxiliary,
    });
}

}

void JtImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* 80 chars for a version string
       1 char for endianness
       4 bytes empty */
    constexpr std::size_t headerSize = 80 + 1 + 4;
    if(data.size() < headerSize) {
        Error{} << "Trade::JtImporter::openData(): file too short, expected at least" << headerSize << "bytes for a header but got" << data.size();
        return;
    }

    const Containers::StringView version = data.prefix(80);
    if(!version.hasSuffix(" \n\r\n "_s)) {
        // TODO print as hex somehow
        Error{} << "Trade::JtImporter::openData(): invalid version string suffix";
        return;
    }

    Debug{} << version.exceptSuffix(" \n\r\n "_s).trimmedSuffix();

    bool has32BitOffset;
    if(version.hasPrefix("Version 10."_s)) // TODO here 10.0 works too, 10.6 as well
        has32BitOffset = false;
    else if(version.hasPrefix("Version 8.0 "_s))
        has32BitOffset = true;
    else {
        Error{} << "Trade::JtImporter::openData(): unsupported version" << version.trimmedSuffix();
        return;
    }

    if(data[80] != 0) {
        Error{} << "Trade::JtImporter::openData(): Big Endian files are not supported, sorry";
        return;
    }

    if(read<Int>(data[81]) != 0) {
        Error{} << "Trade::JtImporter::openData(): Empty Field not empty, what to do?!";
        return;
    }

    // TODO maybe it's just 4B in 8.0? have to branch then
    // TODO not sure which version is in https://github.com/FreeCAD/FreeCAD/blob/196200b1322114bcf7e7d038980e4e0de4a35c42/src/Mod/JtReader/App/JrJt/JtReader.cpp#L66
        // TODO JT 10.6 has it 64 yeh
    // const UnsignedInt tocOffset = read<UnsignedInt>(data[85]);
    const UnsignedLong tocOffset = has32BitOffset ?
        read<UnsignedInt>(data[headerSize]) :
        read<UnsignedLong>(data[headerSize]);
    if(tocOffset + 4 > data.size()) {
        Error{} << "Trade::JtImporter::openData(): TOC offset at" << tocOffset << "out of range for a file of" << data.size() << "bytes";
        return;
    }

    Guid logicalSceneGraphGuid = read<Guid>(data[headerSize + (has32BitOffset ? 4 : 8)]);
    std::size_t logicalSceneGraphOffset = ~std::size_t{};
    Debug{} << "Logical Scene Graph ID:" << logicalSceneGraphGuid;

    /* Read the TOC */
    struct TocEntry32 {
        Guid guid;
        UnsignedInt offset;
        UnsignedInt size;
        UnsignedByte:8;
        UnsignedByte:8;
        UnsignedByte:8;
        UnsignedByte type;
    };
    struct TocEntry {
        /*implicit*/ TocEntry(const TocEntry32& other): guid{other.guid}, offset{other.offset}, size{other.size}, type{other.type} {}
        Guid guid;
        UnsignedLong offset;
        UnsignedInt size;
        UnsignedByte:8;
        UnsignedByte:8;
        UnsignedByte:8;
        UnsignedByte type;
    };
    const Int tocEntryCount = read<Int>(data[tocOffset]);
    const std::size_t tocEntrySize = has32BitOffset ? sizeof(TocEntry32) : sizeof(TocEntry);
    if(tocOffset + 4 + tocEntryCount*tocEntrySize > data.size()) {
        // TODO i suppose this catches negative count too?
        Error{} << "Trade::JtImporter::openData(): TOC offset" << tocOffset << "and size" << 4 + tocEntryCount*tocEntrySize << "out of range for a file of" << data.size() << "bytes";
        return;
    }

    struct SegmentHeader {
        Guid guid;
        UnsignedInt type;
        UnsignedInt size;
    };
    for(Int i = 0; i != tocEntryCount; ++i) {
        const char& entryData = data[tocOffset + 4 + i*tocEntrySize];
        const TocEntry entry = has32BitOffset ?
            read<TocEntry32>(entryData) :
            read<TocEntry>(entryData);

        Debug out;
        out << entry.guid;

        if(entry.type == 1) {
            out << "Logical Scene Graph";
            if(entry.guid == logicalSceneGraphGuid) // TODO test this!
                logicalSceneGraphOffset = entry.offset;
        } else if(entry.type == 2)
            out << "JT B-Rep";
        else if(entry.type == 3)
            out << "PMI Data"; // TODO deprecated???
        else if(entry.type == 4)
            out << "Meta Data";
        else if(entry.type == 6)
            out << "Shape";
        else if(entry.type >= 7 && entry.type <= 16)
            out << "Shape LOD" << Debug::nospace << entry.type - 7;
        else if(entry.type == 17)
            out << "XT B-Rep";
        else if(entry.type == 18)
            out << "Wireframe Representation";
        else if(entry.type == 20)
            out << "ULP";
        else if(entry.type == 23)
            out << "STT";
        else if(entry.type == 24)
            out << "LWPA";
        else if(entry.type == 30)
            out << "MultiXT B-Rep";
        else if(entry.type == 31)
            out << "InfoSegment";
        else if(entry.type == 33)
            out << "STEP B-Rep";
        // TODO fail if unknown?
        out << "(type" << entry.type << Debug::nospace << ")";

        out << "@" << entry.offset << entry.size;

        if(entry.offset + entry.size > data.size()) {
            // TODO mention TOC entry name and # here once not printing the above
            Error{} << "Trade::JtImporter::openData(): segment at" << entry.offset << "and size" << entry.size << "out of range for file of" << data.size() << "bytes";
            return;
        }

        /* The segment should have exactly the same header, validate the
           consistency */
        const SegmentHeader segment = read<SegmentHeader>(data[entry.offset]);
        if(segment.guid != entry.guid) {
            Error{} << "Trade::JtImporter::openData(): TOC entry" << entry.guid << "refers a segment with non-matching GUID" << segment.guid;
            return;
        }
        if(segment.type != entry.type) {
            Error{} << "Trade::JtImporter::openData(): TOC entry" << entry.guid << "refers a segment with non-matching type" << segment.type;
            return;
        }
        if(segment.size != entry.size) {
            Error{} << "Trade::JtImporter::openData(): TOC entry" << entry.guid << "refers a segment with non-matching size" << segment.size;
            return;
        }
    }

    /* Get the scene graph data */
    if(logicalSceneGraphOffset == ~std::size_t{}) {
        Error{} << "Trade::JtImporter::openData(): no TOC entry for a Logical Scene Graph ID" << logicalSceneGraphGuid;
        return;
    }

    // TODO The compression header seems to be only for new files? or not??
    Containers::Array<char> decoded;
    Containers::ArrayView<const char> logicalSceneGraphData;
    if(!has32BitOffset) {
        struct __attribute__((packed)) LogicalElementHeaderCompressed {
            UnsignedInt compressionFlag;
            Int compressedDataLength; // TODO it includes everything after
            UnsignedByte compressionAlgorithm;
            // LogicalElementHeader header; // TODO no, this is compressed already .. IF at all
        };
        const std::size_t offset = logicalSceneGraphOffset + sizeof(SegmentHeader);
        const LogicalElementHeaderCompressed header = read<LogicalElementHeaderCompressed>(data[offset]);
        if(header.compressionFlag == 3 && header.compressionAlgorithm == 3) {
            lzma_stream lzma = LZMA_STREAM_INIT;
            CORRADE_INTERNAL_ASSERT_OUTPUT(lzma_stream_decoder(&lzma, ~std::uint64_t{}, 0) == LZMA_OK);
            lzma.next_in = reinterpret_cast<const UnsignedByte*>(data.data() + offset + sizeof(LogicalElementHeaderCompressed));
            lzma.avail_in = header.compressedDataLength - 1; // TODO lol document why -1 (because the compressionAlgorithm byte is included in the compressed data length, ffs)

            arrayResize(decoded, NoInit, 1024); // TODO uhh can i get the decompressed size somewhere?!
            lzma.next_out = reinterpret_cast<UnsignedByte*>(decoded.data());
            lzma.avail_out = decoded.size();

            for(;;) {
                const lzma_ret out = lzma_code(&lzma, LZMA_RUN);
                if(out == LZMA_STREAM_END)
                    break;
                else CORRADE_INTERNAL_ASSERT(out == LZMA_OK);

                Containers::ArrayView<char> next = arrayAppend(decoded, NoInit, 1024);
                lzma.next_out = reinterpret_cast<UnsignedByte*>(next.data());
                lzma.avail_out = next.size();
            }

            arrayResize(decoded, lzma.total_out);
            logicalSceneGraphData = decoded;
            Debug{} << "Logical scene data bytes:" << decoded.size();
            lzma_end(&lzma);

            Utility::Path::write("decoded.lzma", decoded);

        } else {
            // TODO ?!
            Error{} << "Trade::JtImporter::openData(): non-LZMA-compressed segments not implemented, sorry";
            return;
        }
    } else {
        CORRADE_INTERNAL_ASSERT_UNREACHABLE(); // TODO no idea how to decode v8 files...
    }

    // TODO packing for msvc... (#pragma pack(push, 1) and #pragma pack(pop))
    struct __attribute__((packed)) LogicalElementHeader {
        Int dataSize;
        Guid objectTypeId;
        UnsignedByte objectBaseType; /* FFS, there goes the alignment */
        Int objectId;
    };

    std::size_t offset = 0;
    bool secondEOE = false;
    // std::size_t partitionNodeElementOffset = ~std::size_t{};
    for(;;) {
        const LogicalElementHeader header = read<LogicalElementHeader>(logicalSceneGraphData[offset]);

        /* End Of Elements */
        if(header.objectTypeId == Guid{0xffffffff, 0xffff, 0xffff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}) {
            /* There's just the GUID, nothing else from the
               LogicalElementHeader */
            CORRADE_INTERNAL_ASSERT(header.dataSize == 16);
            offset += header.dataSize + 4; // TODO is this correct at all??
            // offset += sizeof(LogicalElementHeader);
            !Debug{} << "ahh??";
            if(secondEOE)
                break;
            secondEOE = true;
            continue;
        }

        /* GUIDs and names painstakingly copied from the 10.6 spec PDF, in the
           same order */
        Debug out;
        if(header.objectTypeId == Guid{0x10dd1035, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Base Node Element";
        else if(header.objectTypeId == Guid{0x10dd101b, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Group Node Element";
        else if(header.objectTypeId == InstanceNodeElementId)
             out << "Instance Node Element";
        else if(header.objectTypeId == Guid{0x10dd102c, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "LOD Node Element";
        else if(header.objectTypeId == Guid{0xce357245, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1})
             out << "Meta Data Node Element";
        else if(header.objectTypeId == Guid{0xd239e7b6, 0xdd77, 0x4289, 0xa0, 0x7d, 0xb0, 0xee, 0x79, 0xf7, 0x94, 0x94})
             out << "NULL Shape Node Element";
        else if(header.objectTypeId == Guid{0xce357244, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1})
             out << "Part Node Element";
        else if(header.objectTypeId == PartitionNodeElementId)
             out << "Partition Node Element";
        else if(header.objectTypeId == Guid{0x10dd104c, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Range LOD Node Element";
        else if(header.objectTypeId == Guid{0x10dd10f3, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Switch Node Element";
        else if(header.objectTypeId == Guid{0x10dd1059, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Base Shape Node Element";
        else if(header.objectTypeId == Guid{0x98134716, 0x0010, 0x0818, 0x19, 0x98, 0x08, 0x00, 0x09, 0x83, 0x5d, 0x5a})
             out << "Point Set Shape Node Element";
        else if(header.objectTypeId == Guid{0x10dd1048, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Polygon Set Shape Node Element";
        else if(header.objectTypeId == Guid{0x10dd1046, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Polyline Set Shape Node Element";
        else if(header.objectTypeId == Guid{0xe40373c1, 0x1ad9, 0x11d3, 0x9d, 0xaf, 0x00, 0xa0, 0xc9, 0xc7, 0xdd, 0xc2})
             out << "Primitive Set Shape Node Element";
        else if(header.objectTypeId == TriStripSetShapeNodeElementId)
             out << "Tri-Strip Set Shape Node Element";
        else if(header.objectTypeId == Guid{0x10dd107f, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Vertex Shape Node Element";
        else if(header.objectTypeId == Guid{0x10dd1001, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Base Attribute Data";
        else if(header.objectTypeId == Guid{0x10dd1014, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Draw Style Attribute Element";
        else if(header.objectTypeId == GeometricTransformAttributeElementId)
             out << "Geometric Transform Attribute Element";
        else if(header.objectTypeId == Guid{0x10dd1028, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Infinite Light Attribute Element";
        else if(header.objectTypeId == Guid{0x10dd1096, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Light Set Attribute Element";
        else if(header.objectTypeId == Guid{0x10dd10c4, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Linestyle Attribute Element";
        else if(header.objectTypeId == MaterialAttributeElementId)
             out << "Material Attribute Element";
        else if(header.objectTypeId == Guid{0x10dd1045, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Point Light Attribute Element";
        else if(header.objectTypeId == Guid{0x8d57c010, 0xe5cb, 0x11d4, 0x84, 0x0e, 0x00, 0xa0, 0xd2, 0x18, 0x2f, 0x9d})
             out << "Pointstyle Attribute Element";
        else if(header.objectTypeId == Guid{0x10dd1073, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Texture Image Attribute Element";
        else if(header.objectTypeId == Guid{0xaa1b831d, 0x6e47, 0x4fee, 0xa8, 0x65, 0xcd, 0x7e, 0x1f, 0x2f, 0x39, 0xdc})
             out << "Texture Coordinate Generator Attribute Element";
        else if(header.objectTypeId == Guid{0x10dd1106, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "PaletteMap Attribute Element";
        else if(header.objectTypeId == Guid{0xa3cfb921, 0xbdeb, 0x48d7, 0xb3, 0x96, 0x8b, 0x8d, 0x0e, 0xf4, 0x85, 0xa0})
             out << "Mapping Plane Element";
        else if(header.objectTypeId == Guid{0x3e70739d, 0x8cb0, 0x41ef, 0x84, 0x5c, 0xa1, 0x98, 0xd4, 0x00, 0x3b, 0x3f})
             out << "Mapping Cylinder Element";
        else if(header.objectTypeId == Guid{0x72475fd1, 0x2823, 0x4219, 0xa0, 0x6c, 0xd9, 0xe6, 0xe3, 0x9a, 0x45, 0xc1})
             out << "Mapping Sphere Element";
        else if(header.objectTypeId == Guid{0x92f5b094, 0x6499, 0x4d2d, 0x92, 0xaa, 0x60, 0xd0, 0x5a, 0x44, 0x32, 0xcf})
             out << "Mapping TriPlanar Element";
        else if(header.objectTypeId == Guid{0x10dd104b, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Base Property Atom Element";
        else if(header.objectTypeId == DatePropertyAtomElementId)
             out << "Date Property Atom Element";
        else if(header.objectTypeId == IntegerPropertyAtomElementId)
             out << "Integer Property Atom Element";
        else if(header.objectTypeId == FloatingPointPropertyAtomElementId)
             out << "Floating Point Property Atom Element";
        else if(header.objectTypeId == LateLoadedPropertyAtomElementId)
             out << "Late Loaded Property Atom Element";
        else if(header.objectTypeId == Guid{0x10dd1004, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97})
             out << "Object Reference Property Atom Element";
        else if(header.objectTypeId == StringPropertyAtomElementId)
             out << "String Property Atom Element";
        else
            out << header.objectTypeId;

        out << "ID" << header.objectId;
        // TODO i guess put all this into a table indexed by the object ID, and then parse later??

        // TODO useless i guess
        // if(header.objectBaseType == 0)
        //     out << Debug::nospace << ", Base Graph Node Object";
        // else if(header.objectBaseType == 1)
        //     out << Debug::nospace << ", Group Graph Node Object";
        // else if(header.objectBaseType == 2)
        //     out << Debug::nospace << ", Shape Graph Node Object";
        // else if(header.objectBaseType == 3)
        //     out << Debug::nospace << ", Base Attribute Object";
        // else if(header.objectBaseType == 4)
        //     out << Debug::nospace << ", Shape LOD";
        // else if(header.objectBaseType == 5)
        //     out << Debug::nospace << ", Base Property Object";
        // else if(header.objectBaseType == 6)
        //     out << Debug::nospace << ", JT Object Reference Object";
        // else if(header.objectBaseType == 8)
        //     out << Debug::nospace << ", JT Late Loaded Property Object";
        // else if(header.objectBaseType == 9)
        //     out << Debug::nospace << ", JtBase";
        // else if(header.objectBaseType != 255)
        //     out << Debug::nospace << ", Unknown base";

        /* Process common base types, in the order they appear in structure
           inheritance */
        std::size_t propertyOffset = offset + sizeof(LogicalElementHeader);

        /* Node properties */
        if(header.objectBaseType == 0 ||
           header.objectBaseType == 1 ||
           header.objectBaseType == 2)
        {
            struct __attribute__((packed)) BaseNodeData {
                UnsignedByte versionNumber;
                UnsignedInt nodeFlags;
                Int attributeCount;
                /* Then attributeCount*sizeof(Int) attribute IDs */
            };
            const BaseNodeData baseNodeData = read<BaseNodeData>(logicalSceneGraphData[propertyOffset]);
            // TODO check the count isn't over the element size
            propertyOffset += sizeof(BaseNodeData);
            out << Debug::newline << "  attributes" << Containers::arrayCast<const Int>(logicalSceneGraphData.sliceSize(propertyOffset, sizeof(Int)*baseNodeData.attributeCount));
            propertyOffset += sizeof(Int)*baseNodeData.attributeCount;
        }

        /* Group properties */
        if(header.objectBaseType == 1) {
            struct __attribute__((packed)) GroupNodeData {
                /* BaseNodeData (with variable attribute count) before */
                UnsignedByte versionNumber;
                Int childCount;
                /* Then childCount*sizeof(Int) child IDs */
            };
            const GroupNodeData groupNodeData = read<GroupNodeData>(logicalSceneGraphData[propertyOffset]);
            // TODO check the count isn't over the element size
            propertyOffset += sizeof(GroupNodeData);
            out << Debug::newline << "  children" << Containers::arrayCast<const Int>(logicalSceneGraphData.sliceSize(propertyOffset, sizeof(Int)*groupNodeData.childCount));
            propertyOffset += sizeof(Int)*groupNodeData.childCount;
        }

        /* Shape node properties */
        if(header.objectBaseType == 2) {
            struct __attribute__((packed)) ShapeNodeData {
                /* BaseNodeData (with variable attribute count) before */
                UnsignedByte versionNumber;
                Float untransformedBbox[3*2];
                Float area;
                Int vertexCountRange[2];
                Int nodeCountRange[2];
                Int polygonCountRange[2];
                UnsignedInt size;
                Float compressionLevel;
            };
            // const ShapeNodeData shapeNodeData = read<ShapeNodeData>(logicalSceneGraphData[propertyOffset]);
            // TODO check the count isn't over the element size
            propertyOffset += sizeof(ShapeNodeData);
            // TODO check the struct isn't over the element size
        }

        /* Attribute properties */
        if(header.objectBaseType == 3) {
            struct __attribute__((packed)) BaseAttributeData {
                UnsignedByte versionNumber; /* if 2, there's a 4B pallete index after all other data */
                UnsignedByte stateFlags;
                UnsignedInt fieldInhibitFlags;
                UnsignedInt fieldFinalFlags;
            };
            // TODO check the struct isn't over the element size
            // const BaseAttributeData baseAttributeData = read<BaseAttributeData>(logicalSceneGraphData[propertyOffset]);
            propertyOffset += sizeof(BaseAttributeData);

            // TODO ugh actually this is only after the rest, such as a matrix...
            // /* Palette index, if present */
            // if(baseAttributeData.versionNumber == 2)
            //     propertyOffset += sizeof(Int);
        }

        /* Property properties */
        if(header.objectBaseType == 5 ||
           header.objectBaseType == 8)
        {
            struct __attribute__((packed)) BasePropertyAtomData {
                UnsignedByte versionNumber;
                UnsignedInt stateFlags;
            };
            // TODO check the struct isn't over the element size
            // const BasePropertyAtomData basePropertyAtomData = read<BasePropertyAtomData>(logicalSceneGraphData[propertyOffset]);
            propertyOffset += sizeof(BasePropertyAtomData);
        }

        /* Process a subset of known GUIDs. Ordered the same as the table in
           the 10.6 spec PDF. */

        /* Instance node */
        if(header.objectTypeId == InstanceNodeElementId) {
            struct __attribute__((packed)) InstanceNodeData {
                UnsignedByte versionNumber;
                UnsignedInt childNodeObjectId;
            };
            // TODO check the struct isn't over the element size
            const InstanceNodeData instanceNodeData = read<InstanceNodeData>(logicalSceneGraphData[propertyOffset]);

            out << Debug::newline << "  child" << instanceNodeData.childNodeObjectId;

        /* Partition Node */
        } else if(header.objectTypeId == PartitionNodeElementId) {
            // const Int partitionFlags = read<Int>(logicalSceneGraphData[propertyOffset]);
            propertyOffset += sizeof(Int);

            propertyOffset += 1; // TODO why?! where am i off by one???

            const std::size_t stringSize = read<Int>(logicalSceneGraphData[propertyOffset]);
            propertyOffset += sizeof(Int);

            // TODO check the count isn't over the element size
            Containers::String filename{NoInit, stringSize};
            for(std::size_t i = 0; i != stringSize; ++i) {
                // TODO document it's a stupid ascii conversion
                filename[i] = logicalSceneGraphData[propertyOffset];
                propertyOffset += sizeof(UnsignedShort);
            }

            struct PartitionNodeData {
                /* BaseNodeData and GroupNodeData (with variable attribute /
                   child count) before */
                /* 1 byte padding (or versionNumber?) that isn't in spec (!?),
                   4-byte partitionFlags, 4-byte string size and then 2-byte
                   string chars */
                Range3D untransformedBbox;
                Float area;
                Range1Di vertexCount;
                Range1Di nodeCount;
                Range1Di polygonCount;
            };
            const PartitionNodeData partitionNodeData = read<PartitionNodeData>(logicalSceneGraphData[propertyOffset]);

            out << filename << Debug::newline
                << "  bounds:" << partitionNodeData.untransformedBbox << Debug::newline
                << "  nodes:" << partitionNodeData.nodeCount << Debug::newline
                << "  vertices:" << partitionNodeData.vertexCount;

        /* Geometric Transform Attribute */
        } else if(header.objectTypeId == GeometricTransformAttributeElementId) {
            struct __attribute__((packed)) GeometricTransformAttributeData {
                /* BaseAttributeData before */
                UnsignedByte versionNumber;
                UnsignedShort storedValues; /* 0xffff means all components */
                /* popcount(storedValues) Float values after, and then optional
                   palette index */
            };
            const GeometricTransformAttributeData geometricTransformAttributeData = read<GeometricTransformAttributeData>(logicalSceneGraphData[propertyOffset]);
            propertyOffset += sizeof(GeometricTransformAttributeData);

            Matrix4d data{Math::IdentityInit};
            UnsignedShort mask = 0x8000;
            for(UnsignedInt i = 0; i != 16; ++i) {
                if(geometricTransformAttributeData.storedValues & mask) {
                    data.data()[i] = read<Double>(logicalSceneGraphData[propertyOffset]);
                    propertyOffset += sizeof(Double);
                }
                mask >>= 1;
            }

            /* JT uses row-major storage and LTR multiplication, so for RTL and
               column major it's the same representation */
            out << Debug::newline << Matrix4{data};

        /* Material Attribute */
        } else if(header.objectTypeId == MaterialAttributeElementId) {
            struct __attribute__((packed)) MaterialAttributeData {
                /* BaseAttributeData before */
                UnsignedByte versionNumber;
                UnsignedShort dataFlags;
                Float ambientColor[4];
                Float diffuseColor[4];
                Float specularColor[4];
                Float emissionColor[4];
                Float shininess;
                Float reflectivity;
                Float bumpiness;
                /* Optional palette index after */
            };
            const MaterialAttributeData materialAttributeData = read<MaterialAttributeData>(logicalSceneGraphData[propertyOffset]);

            out << Debug::newline << "  ambient color:" << Math::pack<Color4ub>(Color4{materialAttributeData.ambientColor})
                << Debug::newline << "  diffuse color:" << Math::pack<Color4ub>(Color4{materialAttributeData.diffuseColor})
                << Debug::newline << "  specular color:" << Math::pack<Color4ub>(Color4{materialAttributeData.specularColor})
                << Debug::newline << "  emission color:" << Math::pack<Color4ub>(Color4{materialAttributeData.emissionColor})
                << Debug::newline << "  shininess:" << materialAttributeData.shininess
                << Debug::newline << "  reflectivity:" << materialAttributeData.reflectivity
                << Debug::newline << "  bumpiness:" << materialAttributeData.bumpiness;

        /* Tri-Strip Set Shape Node */
        } else if(header.objectTypeId == TriStripSetShapeNodeElementId) {
            struct __attribute__((packed)) VertexShapeData {
                UnsignedByte versionNumber;
                UnsignedLong vertexBinding;
            };
            const VertexShapeData vertexShapeData = read<VertexShapeData>(logicalSceneGraphData[propertyOffset]);

            out << Debug::newline << "  vertex binding:" << Debug::hex << VertexBindings{vertexShapeData.vertexBinding};

        /* Date Property Atom Element */
        } else if(header.objectTypeId == DatePropertyAtomElementId) {
            struct __attribute__((packed)) DatePropertyAtomData {
                /* BasePropertyAtomData before */
                UnsignedByte versionNumber;
                UnsignedShort year, month, day, hour, minute, second;
            };
            // TODO check the struct isn't over the element size
            const DatePropertyAtomData datePropertyAtomData = read<DatePropertyAtomData>(logicalSceneGraphData[propertyOffset]);

            /* Month is in range (0, 11) for some reason, day not... */
            out << datePropertyAtomData.year << Debug::nospace << "-" << Debug::nospace << datePropertyAtomData.month + 1 << Debug::nospace << "-" << Debug::nospace << datePropertyAtomData.day << datePropertyAtomData.hour << Debug::nospace << ":" << Debug::nospace << datePropertyAtomData.minute << Debug::nospace << ":" << Debug::nospace << datePropertyAtomData.second;

        /* Integer Property Atom Element */
        } else if(header.objectTypeId == IntegerPropertyAtomElementId) {
            struct __attribute__((packed)) IntegerPropertyAtomData {
                /* BasePropertyAtomData before */
                UnsignedByte versionNumber;
                Int value;
            };
            // TODO check the struct isn't over the element size
            const IntegerPropertyAtomData integerPropertyAtomData = read<IntegerPropertyAtomData>(logicalSceneGraphData[propertyOffset]);

            out << integerPropertyAtomData.value;

        /* Floating-Point Property Atom Element */
        } else if(header.objectTypeId == FloatingPointPropertyAtomElementId) {
            struct __attribute__((packed)) FloatingPointPropertyAtomData {
                /* BasePropertyAtomData before */
                UnsignedByte versionNumber;
                Float value;
            };
            // TODO check the struct isn't over the element size
            const FloatingPointPropertyAtomData floatingPointPropertyAtomData = read<FloatingPointPropertyAtomData>(logicalSceneGraphData[propertyOffset]);

            out << floatingPointPropertyAtomData.value;

        /* Late Loaded Property Atom Element */
        } else if(header.objectTypeId == LateLoadedPropertyAtomElementId) {
            struct __attribute__((packed)) LateLoadedPropertyAtomData {
                /* BasePropertyAtomData before */
                UnsignedByte versionNumber;
                Guid segmentId;
                Int segmentType;
                Int payloadObjectId;
                Int reserved;
            };
            // TODO check the struct isn't over the element size
            const LateLoadedPropertyAtomData lateLoadedPropertyAtomData = read<LateLoadedPropertyAtomData>(logicalSceneGraphData[propertyOffset]);

            out << Debug::newline << "  ID" << lateLoadedPropertyAtomData.segmentId << "(type" << lateLoadedPropertyAtomData.segmentType << Debug::nospace << ")" << lateLoadedPropertyAtomData.payloadObjectId;

        /* String Property Atom Element */
        } else if(header.objectTypeId == StringPropertyAtomElementId) {
            struct __attribute__((packed)) StringPropertyAtomData {
                /* BasePropertyAtomData before */
                UnsignedByte versionNumber;
                UnsignedInt stringSize;
            };
            // TODO check the struct isn't over the element size
            const StringPropertyAtomData stringPropertyAtomData = read<StringPropertyAtomData>(logicalSceneGraphData[propertyOffset]);
            propertyOffset += sizeof(StringPropertyAtomData);

            // TODO check the count isn't over the element size
            Containers::String string{NoInit, stringPropertyAtomData.stringSize};
            for(std::size_t i = 0; i != stringPropertyAtomData.stringSize; ++i) {
                // TODO document it's a stupid ascii conversion
                string[i] = logicalSceneGraphData[propertyOffset];
                propertyOffset += sizeof(UnsignedShort);
            }

            out << string;
        }

        // TODO for fucks sake, this took a while
        offset += /*sizeof(LogicalElementHeader) + */header.dataSize + 4;
        // !Debug{} << offset;
        // TODO check bounds, header has to fit next
    }

    /* Property table, right after all the elements */
    // TODO why have to skip TWO end of elements?!
    {
        struct __attribute__((packed)) PropertyTableData {
            UnsignedShort versionNumber;
            Int elementPropertyTableCount;
        };
        // TODO check the struct isn't over the element size
        const PropertyTableData propertyTableData = read<PropertyTableData>(logicalSceneGraphData[offset]);
        offset += sizeof(PropertyTableData);

        for(Int i = 0; i != propertyTableData.elementPropertyTableCount; ++i) {
            // offset += 3; // TODO uh?

            const Int objectId = read<Int>(logicalSceneGraphData[offset]);
            offset += sizeof(Int);

            Containers::Array<Containers::Pair<Int, Int>> keysValues;
            for(;;) {
                const Int atomObjectId = read<Int>(logicalSceneGraphData[offset]);
                offset += sizeof(Int);
                if(atomObjectId == 0)
                    break;

                const Int valueObjectId = read<Int>(logicalSceneGraphData[offset]);
                offset += sizeof(Int);
                arrayAppend(keysValues, InPlaceInit, atomObjectId, valueObjectId);
            }

            Debug{} << "Object" << objectId << "property table:" << keysValues;
        }
    }

    // TODO possibly copy the thing
}

UnsignedInt JtImporter::doMeshCount() const {
    return 0; // TODO
}

Containers::Optional<MeshData> JtImporter::doMesh(const UnsignedInt id, UnsignedInt) {
    return {}; // TODO
}

}}

CORRADE_PLUGIN_REGISTER(JtImporter, Magnum::Trade::JtImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
