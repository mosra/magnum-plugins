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

#include "LdrawImporter.h"

#include <unordered_map>
#include <Corrade/Containers/ArrayTuple.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringStlHash.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/SceneData.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;
using namespace Math::Literals;

struct Vertex {
    Vector3 position;
    Vector3 normal;
};

struct Mesh {
    // TODO store names also, test 'em
    // Containers::String name;

    /* Points to State::vertices and indices. If the range is empty, there
       aren't any actual mesh data. */
    UnsignedInt vertexBegin = 0, vertexEnd = 0;
    UnsignedInt indexBegin = 0, indexEnd = 0;
    // TODO also color indices / vertices, eventually
};

struct Material {
    Color4 color;
    // TODO name maybe also, some other props like "rubber"
};

struct Object {
    Int parent;
    Matrix4 transformation;
};

struct LdrawImporter::State {
    Containers::Array<Vertex> vertices;
    Containers::Array<UnsignedInt> indices;
    Containers::Array<Mesh> meshes;
    Containers::Array<Material> materials;

    Containers::Array<Object> objects;
    // TODO there's just ~300 materials at most, could use a Short ... or maybe a Byte as not all may get used anyway
    Containers::Array<Containers::Triple<UnsignedInt, UnsignedInt, Int>> objectMeshesMaterials;
};

LdrawImporter::LdrawImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

LdrawImporter::~LdrawImporter() = default;

// TODO we're looking in LDRAWDIR so don't strictly need to know the current dir
    // TODO implement that tho
ImporterFeatures LdrawImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool LdrawImporter::doIsOpened() const {
    return !!_state;
}

void LdrawImporter::doClose() {
    _state = {};
}

namespace {

    // TODO document these are only temp
struct File {
    /* If non-empty, the file is part of the main Multi-Part Document and is
       not parsed yet as at that point it's not yet known which files are
       embedded and which are external */
    // TODO uhhh this is a wasted field once parsed... make a union? ugh
    Containers::StringView contents;

    /* Points to State::meshes. If ~UnsignedInt{}, there is no mesh data in the
       file. */
    UnsignedInt meshId = ~UnsignedInt{};

    /* Points to State::subFiles. If the range is empty, there aren't any
       nested files. */
    UnsignedInt subFileBegin = 0, subFileEnd = 0;
};

struct SubFile {
    Matrix4 transformation;
    /* Points back to the files array */
    UnsignedInt fileId;
    /* LDRaw-specific color index, *not* a concrete index into the
       state.materials array. Is turned into a concrete index by a lookup in
       the color map or by overriding a default inside populateScene(). */
    UnsignedInt colorIndex;
};

Containers::Optional<File> parse(Containers::Array<Vertex>& vertices, Containers::Array<UnsignedInt>& indices, Containers::Array<Mesh>& meshes, Containers::Array<File>& files, std::unordered_map<Containers::String, UnsignedInt>& fileMap, Containers::Array<SubFile>& subFiles, Containers::StringView filePath, Containers::StringView ldrawDir, Containers::StringView pathPrefix, /*mutable*/ Containers::StringView in) {
    /* Lines referencing nested files can be arbitrarily mixed with lines
       describing vertex data, so cannot just append directly to the global
       vertices / indices / subFiles arrays. Instead populate local arrays and
       append them to the global ones at the very end so everything belonging
       to a particular file is localized together. */
    Containers::Array<Vertex> fileVertices;
    Containers::Array<UnsignedInt> fileIndices;
    Containers::Array<SubFile> fileSubFiles;

    /* Parse lines according to https://www.ldraw.org/article/218.html */
    while(in) {
        // TODO change to just find() when that's the default
        const Containers::StringView lineEnd = in.findOr('\n', in.end());
        /* Trim leading whitespace, it can be just a space or a tab. There can
           be a CRLF after, get rid of it as well. */
        Containers::StringView line = in.prefix(lineEnd.begin())
            .trimmedPrefix(" \t"_s)
            .trimmedSuffix("\r"_s);
        in = in.suffix(lineEnd.end());

        /* Command */
        // TODO test w/ no command at all, or just a number 0 alone, should both work
        const Containers::StringView commandEnd = line.findAnyOr(" \t", line.end());
        const Containers::StringView command = line.prefix(commandEnd.begin());
        line = line.suffix(commandEnd.end());
        // TODO hum, maybe some "consume" where it returns the prefix and updates self with suffix?

        /* Empty line, comment or meta command, ignore for now. The `0 FILE`
           commands were parsed upfront already. */
        if(!command || command == "0"_s) {
            continue; // TODO test with empty lines, lines with just whitespace,\r\n also

        /* External file reference */
        } else if(command == "1"_s) {
            /* Color */
            const Containers::StringView colorEnd = line
                // TODO remove the trimmedPrefix once findAny consumes all
                .trimmedPrefix(" \t"_s)
                .findAnyOr(" \t", line.end());
            const Containers::StringView color = line.prefix(colorEnd.begin());
            line = line.suffix(colorEnd.end());
            // TODO use just operator bool once implemented
            UnsignedInt colorIndex;
            if(Utility::String::parseDecimal(color, colorIndex) != Utility::String::ParseState::Success) {
                // TODO need current filename + a begin pointer to produce file:line:column
                // TODO maybe if it's empty have a different message, like "line too short"
                Error{} << "LdrawImporter::openData(): invalid integer literal" << color << "at TODO";
                return {};
            }

            /* Translation and rotation */
            Float transformation[12];
            for(std::size_t i = 0; i != Containers::arraySize(transformation); ++i) {
                const Containers::StringView numberEnd = line
                    // TODO remove the trimmedPrefix once findAny consumes all
                    .trimmedPrefix(" \t"_s)
                    .findAnyOr(" \t", line.end());
                const Containers::StringView number = line.prefix(numberEnd.begin());
                line = line.suffix(numberEnd.end());

                // TODO use just operator bool once implemented
                if(Utility::String::parseFloat(number, transformation[i]) != Utility::String::ParseState::Success) {
                    // TODO need current filename + a begin pointer to produce file:line:column
                    // TODO maybe if it's empty have a different message, like "line too short"
                    Error{} << "LdrawImporter::openData(): invalid float literal" << number << "at TODO";
                    return {};
                }
            }

            /* Filename is the rest of the line, with leading and trailing
               whitespace trimmed. Try various path prefixes and variants until
               there is a match. */
            // TODO fail right away if there's no filename
            // TODO use just trimmedSuffix once findAny consumes all
                // TODO document this \/ replacement
            const Containers::String filename = Utility::String::replaceAll(line.trimmed(" \t"), '\\', '/');

            const auto findParseFile = [&](const Containers::StringView basePath, const Containers::StringView filename) -> Containers::Optional<UnsignedInt> {
                Containers::Optional<UnsignedInt> file;

                /* Look into the map first if it's there. We might have parsed
                   it already or it's one of the embedded files in a Multi-Part
                   Document. */
                auto found = fileMap.find(filename);
                if(found != fileMap.end()) {
                    /* It's one of the embedded files in a Multi-Part Document,
                       parse it and replace the entry with the parsed state */
                    if(const Containers::StringView contents = files[found->second].contents) {
                        if(const Containers::Optional<File> parsed = parse(
                            vertices,
                            indices,
                            meshes,
                            files,
                            fileMap,
                            subFiles,
                            filePath,
                            ldrawDir,
                            Utility::Path::path(filename), contents)
                        )
                            files[found->second] = *parsed;
                        else return ~UnsignedInt{};
                    }

                    /* In either case now it's already parsed, return the ID */
                    return found->second;
                }

                /* If it's found on the filesystem, load & parse it, and add it
                   to the file map */
                const Containers::String fullFilename = Utility::Path::join(basePath, filename);
                if(Utility::Path::exists(fullFilename)) {
                    // !Debug{} << fullFilename;
                    const Containers::Optional<Containers::String> contents = Utility::Path::readString(fullFilename);
                    if(!contents) {
                        Error{} << "LdrawImporter::openData(): cannot read" << fullFilename;
                        return ~UnsignedInt{};
                    }
                    if(const Containers::Optional<File> parsed = parse(
                        vertices,
                        indices,
                        meshes,
                        files,
                        fileMap,
                        subFiles,
                        filePath,
                        ldrawDir,
                        Utility::Path::path(filename), *contents)
                    ) {
                        const UnsignedInt fileId = files.size();
                        fileMap.emplace(filename, fileId);
                        arrayAppend(files, *parsed);
                        return fileId;
                    } else return ~UnsignedInt{};
                }

                /* All errors above returned ~UnsignedInt{}. Return a NullOpt
                   to signalize not an error but that a different filename
                   should be tried. */
                return {};
            };

            /* Try to find and parse the file */
            Containers::Optional<UnsignedInt> foundFile;
            // TODO some filenames use backslashes also, test w/ those; but the original file can have those embedded, so cannot just test with only one i guess?
                // TODO or normalize??
            if(!(foundFile = findParseFile(pathPrefix, filename)) &&
               !(foundFile = findParseFile(ldrawDir, Utility::Path::join("parts", filename))) &&
               !(foundFile = findParseFile(ldrawDir, Utility::Path::join("p", filename))) &&
               !(foundFile = findParseFile(ldrawDir, Utility::Path::join("models", filename)))
            ) {
                Error{} << "LdrawImporter::openData(): cannot find" << filename << "in" << pathPrefix << "or in the parts library at" << ldrawDir;
                return {};
            }
            /* If reading or parsing failed, bail. The nested findParseFile()
               or parse() call already printed an error message. */
            if(foundFile == ~UnsignedInt{})
                return {};

            /* Add the parsed file into the sub-files list. Its nested files,
               if any, are already in the list. */
            // TODO actually maybe don't if it has no subfiles or meshes (i.e., containing just lines or other stuff we don't support)
            arrayAppend(fileSubFiles, InPlaceInit,
                /* Translation is in the first three numbers, rotation in the
                   remaining 3x3 */
                Matrix4{Matrix4x3{
                    Vector3{transformation[3], transformation[6], transformation[9]},
                    Vector3{transformation[4], transformation[7], transformation[10]},
                    Vector3{transformation[5], transformation[8], transformation[11]},
                    Vector3{transformation[0], transformation[1], transformation[2]}}},
                *foundFile, colorIndex);

        /* Line or optional line, skip for now */
        /** @todo implement these, creating a third (fourth?) mesh */
        } else if(command == "2"_s || command == "5"_s) {
            continue;

        /* Triangle or quad */
        } else if(command == "3"_s || command == "4"_s) {
            /* Color */
            const Containers::StringView colorEnd = line
                // TODO remove the trimmedPrefix once findAny consumes all
                    // TODO oh wait but the extra spaces stay in the line and it's added to the `color` variable, right?
                .trimmedPrefix(" \t"_s)
                .findAnyOr(" \t", line.end());
            // const Containers::StringView color = line.prefix(colorEnd.begin()); TODO
            // TODO parse the color, if it's not the default color put the vertices into a separate vertex-colored mesh
            line = line.suffix(colorEnd.end());

            /* Three or four points */
            Float coordinates[3*4];
            const std::size_t pointCount = command == "3"_s ? 3 : 4;
            for(std::size_t i = 0; i != 3*pointCount; ++i) {
                const Containers::StringView numberEnd = line
                    // TODO remove the trimmedPrefix once findAny consumes all
                        // TODO oh wait but the extra spaces stay in the line and it's added to the `number` variable, right?
                    .trimmedPrefix(" \t"_s)
                    .findAnyOr(" \t", line.end());
                const Containers::StringView number = line.prefix(numberEnd.begin());
                line = line.suffix(numberEnd.end());

                // TODO use just operator bool once implemented
                if(Utility::String::parseFloat(number, coordinates[i]) != Utility::String::ParseState::Success) {
                    // TODO need current filename + a begin pointer to produce file:line:column
                    // TODO maybe if it's empty have a different message, like "line too short"
                    Error{} << "LdrawImporter::openData(): invalid float literal" << number << "at TODO";
                    return {};
                }
            }


            /* Calculate a flat normal. Quads should be planar, so we don't
               need to take the fourth point into account at all. */
            const Vector3 a{coordinates[0], coordinates[1], coordinates[2]};
            const Vector3 b{coordinates[3], coordinates[4], coordinates[5]};
            const Vector3 c{coordinates[6], coordinates[7], coordinates[8]};
            const Vector3 normal = Math::cross(b - a, c - a);

            /* Add the parsed vertex data */
            const UnsignedInt vertexOffset = fileVertices.size();
            // TODO use colorVertices if color is special
            arrayAppend(fileVertices, Containers::arrayView<Vertex>({
                {a, normal},
                {b, normal},
                {c, normal},
                {{coordinates[9], coordinates[10], coordinates[11]}, normal},
            }).prefix(pointCount));

            /* Generate indices. They're relative to the file vertex offset,
               not to the whole array. */
            // if(pointCount == 4)
            //     arrayAppend(fileIndices, {
            //         vertexOffset + 0,
            //         vertexOffset + 2,
            //         vertexOffset + 3,
            //         });
            arrayAppend(fileIndices, pointCount == 3 ? Containers::arrayView({
                vertexOffset + 0,
                vertexOffset + 1,
                vertexOffset + 2,
                // TODO everything is twice to have it double-sided with correct normals, fix...
                // vertexOffset + 2*0 + 1,
                // vertexOffset + 2*2 + 1,
                // vertexOffset + 2*1 + 1,
            }) : Containers::arrayView({
                vertexOffset + 0,
                vertexOffset + 1,
                vertexOffset + 2,
                vertexOffset + 0,
                vertexOffset + 2,
                vertexOffset + 3,
                // TODO here too
                // vertexOffset + 2*0 + 1,
                // vertexOffset + 2*1 + 1,
                // vertexOffset + 2*2 + 1,
                // vertexOffset + 2*0 + 1,
                // vertexOffset + 2*3 + 1,
                // vertexOffset + 2*2 + 1,
            }));


        /* There shouldn't be anythíng else */
        } else {
            // TODO file/line/column
            Error{} << "LdrawImporter::openData(): invalid command" << command << "at TODO";
            // return {}; TODO
            continue;
        }
    }

    /* Add the collected subfiles and reference them from the file */
    File file;
    file.subFileBegin = subFiles.size();
    file.subFileEnd = file.subFileBegin + fileSubFiles.size();
    // TODO optionally skip if no subfiles and no meshes? i.e., if it contains just lines or sth
    arrayAppend(subFiles, fileSubFiles);

    /* If we have any mesh data, add a mesh and reference it from the file */
    if(!fileVertices.isEmpty()) {
        file.meshId = meshes.size();

        Mesh mesh;
        mesh.vertexBegin = vertices.size();
        mesh.vertexEnd = mesh.vertexBegin + fileVertices.size();
        mesh.indexBegin = indices.size();
        mesh.indexEnd = mesh.indexBegin + fileIndices.size();
        arrayAppend(meshes, mesh);
        arrayAppend(vertices, fileVertices);
        arrayAppend(indices, fileIndices);
    }

    return file;
}

void populateScene(std::unordered_map<UnsignedInt, Containers::Pair<UnsignedInt, Material>>& colorMap, Containers::Array<Material>& materials, const Containers::ArrayView<const File> files, const Containers::ArrayView<const SubFile> subFiles, Containers::Array<Object>& objects, Containers::Array<Containers::Triple<UnsignedInt, UnsignedInt, Int>>& objectMeshesMaterials, const Int parent, const Matrix4& transformation, const UnsignedInt colorIndex, const File& file) {
    const UnsignedInt objectId = objects.size();

    Object object;
    object.parent = parent;
    // TODO not all objects have a non-trivial transform, make a separate mapping?
    object.transformation = transformation;
    arrayAppend(objects, object);

    /* If the file has a mesh, add it to the mesh/material assignment list */
    if(file.meshId != ~UnsignedInt{}) {
        /* Resolve the color index */
        auto found = colorMap.find(colorIndex);
        if(found == colorMap.end())
            found = colorMap.find(16); // TODO test this, warn

        /* If the color isn't used yet, add it to the materials */
        if(found->second.first() == ~UnsignedInt{}) {
            found->second.first() = materials.size();
            arrayAppend(materials, found->second.second());
        }

        arrayAppend(objectMeshesMaterials, InPlaceInit, objectId, file.meshId, Int(found->second.first()));
    }

    for(UnsignedInt i = file.subFileBegin; i != file.subFileEnd; ++i)
        populateScene(
            colorMap,
            materials,
            files,
            subFiles,
            objects,
            objectMeshesMaterials,
            objectId,
            subFiles[i].transformation,
            /* If the subfile doesn't have a non-default color specified, use
               the parent */
            // TODO describe what this, maybe have a constant instead of hardcoding 16
            subFiles[i].colorIndex == 16 ? colorIndex : subFiles[i].colorIndex,
            files[subFiles[i].fileId]);
}

}

void LdrawImporter::doOpenData(Containers::Array<char>&& data, DataFlags) {
    // TODO fail early if LDRAWDIR is not defined
    /* This gets eventually moved to _state if everything goes well */
    Containers::Pointer<State> state{InPlaceInit};

    /* Parse colors from the main config file in LDRAWDIR. Theoretically it can
       happen that every file opening uses a different LDRAWDIR, thus
       potentially different colors, so just parse them every time to not use
       stale ones. This also means out of the 300+ colors we can keep only the
       entries that are actually used by the model and throw away the rest.

       The value stores index into the state.materials array, which is
       ~UnsignedInt{} initially and gets filled with a concrete index by
       populateScene() as soon as the color is used in the file. */
    std::unordered_map<UnsignedInt, Containers::Pair<UnsignedInt, Material>> colorMap;
        // TODO uhh should only keep the materials that are actually used!
    {
        // TODO use a config value for LDRAWDIR, cache it
        // TODO also maybe have some automatic fallback paths, like next to the opened file?
            // TODO also maybe have a way to override the config filename, even to an absolute location
        const Containers::Optional<Containers::String> config = Utility::Path::readString(Utility::Path::join(std::getenv("LDRAWDIR"), "LDConfig.ldr"));
        if(!config) {
            Error{} << "LdrawImporter::openData(): cannot parse default color values";
            return;
        }

        /* Parse lines according to https://www.ldraw.org/article/299 */
        Containers::StringView in = *config;
        while(in) {
            // TODO change to just find() when that's the default
            const Containers::StringView lineEnd = in.findOr('\n', in.end());
            /* Trim leading whitespace, it can be just a space or a tab. There
               can be a CRLF after, get rid of it as well. */
            Containers::StringView line = in.prefix(lineEnd.begin())
                .trimmedPrefix(" \t"_s)
                .trimmedSuffix("\r"_s);
            in = in.suffix(lineEnd.end());

            /* We're interested only in colors from the config file. The syntax
               is `0 !COLOUR name CODE x VALUE v` with other keywords after for
               edges, alpha, etc., right now we only parse RGB colors and their
               codes. */
            // TODO test the tab after too
            if(!line.hasPrefix("0 !COLOUR "_s) && !line.hasPrefix("0 !COLOUR\t"_s))
                continue;
            line = line.exceptPrefix("0 !COLOUR"_s).trimmedPrefix(" \t"_s);

            /* Extract the name. It's assumed to not have any spaces inside,
               otherwise we'd have to check all the way until the CODE
               keyword */
            // TODO test names with spaces here
            const Containers::StringView nameEnd = line
                .findAnyOr(" \t", line.end());
            const Containers::StringView name = line.prefix(nameEnd.begin());
            // TODO remove the trimmedPrefix once findAny consumes all
            line = line.suffix(nameEnd.end()).trimmedPrefix(" \t"_s);

            if(!line.hasPrefix("CODE "_s) && !line.hasPrefix("CODE\t"_s)) {
                // TODO file/line, maybe have some details why (expected CODE...?)
                Error{} << "LdrawImporter::openData(): invalid color line at TODO";
                return;
            }
            line = line.exceptPrefix("CODE"_s).trimmedPrefix(" \t"_s);

            /* Extract the code */
            // TODO test names with spaces here
            const Containers::StringView codeEnd = line
                .findAnyOr(" \t", line.end());
            const Containers::StringView code = line.prefix(codeEnd.begin());
            // TODO remove the trimmedPrefix once findAny consumes all
            line = line.suffix(codeEnd.end()).trimmedPrefix(" \t"_s);
            UnsignedInt index;
            // TODO use just operator bool once implemented
            if(Utility::String::parseDecimal(code, index) != Utility::String::ParseState::Success) {
                // TODO need current filename + a begin pointer to produce file:line:column
                // TODO maybe if it's empty have a different message, like "line too short"
                Error{} << "LdrawImporter::openData(): invalid integer literal" << code << "at TODO";
                return;
            }

            if(!line.hasPrefix("VALUE "_s) && !line.hasPrefix("VALUE\t"_s)) {
                // TODO file/line, maybe have some details why (expected VALUE...?)
                Error{} << "LdrawImporter::openData(): invalid color line at TODO";
                return;
            }
            line = line.exceptPrefix("VALUE"_s).trimmedPrefix(" \t"_s);

            /* Extract the value */
            // TODO test names with spaces here
            const Containers::StringView valueEnd = line
                .findAnyOr(" \t", line.end());
            const Containers::StringView value = line.prefix(valueEnd.begin());
            UnsignedInt color;
            // TODO use just operator bool once implemented
                // TODO test both # and 0x
                    // TODO this allows neither # or 0x, that should be disallowed, fix in the parser
                    // TODO it also allow non-six-char values, what to do?
            if(Utility::String::parseHexadecimal(value, color, Utility::String::ParseHexadecimalFlag::AllowBasePrefix| Utility::String::ParseHexadecimalFlag::AllowHashPrefix) != Utility::String::ParseState::Success) {
                // TODO need current filename + a begin pointer to produce file:line:column
                // TODO maybe if it's empty have a different message, like "line too short"
                Error{} << "LdrawImporter::openData(): invalid color literal" << color << "at TODO";
                return;
            }

            /* Add to the color map. The ~UnsignedInt{} gets updated to a
               concrete index inside the state.materials array by
               populateScene() once the color is used. */
            if(colorMap.find(index) == colorMap.end())
                colorMap.emplace(index, Containers::pair(~UnsignedInt{}, Material{Color3::fromLinearRgbInt(color)}));
        }

        // TODO check that the colormap has the default color (16) at least
    }

    /* These is all just temporary. List of parsed files, with fileMap mapping
       them to filenames */
    Containers::Array<File> files;
    /* The filenames are always relative to the top-level file or to the root
       of LDRAWDIR */ // TODO make sure that is held
    std::unordered_map<Containers::String, UnsignedInt> fileMap;
    /* Nested files referenced from File::subFileBegin and subFileEnd above */
    Containers::Array<SubFile> subFiles;

    /* If the file starts with 0 FILE, it's a Multi-Part Document, split it
       into parts first and use the first part as the top-level file. Otherwise
       the whole contents get treated as a top-level file. */
    /** @todo According to the spec at https://www.ldraw.org/article/47.html
        the MPD file is allowed to *not* start with 0 FILE, but the same page
        also says that any other content (except for comment lines) before the
        first 0 FILE is considered an error. I don't know what to take from
        that so just do the simplest thing and check for 0 FILE to be the first
        line. Common files from the wild seem to start with that, if parsing
        for `*.mpd` files fails with an embedded file not being found, this
        logic has to be reworked. */
    Containers::StringView topLevelFile;
    {
        Containers::StringView in = Containers::StringView{data}.trimmedPrefix(" \t"_s);
        // TODO document that technicallly it doesn't have to start with that but we don't care
        if(in.hasPrefix("0 FILE "_s) || in.hasPrefix("0 FILE\t"_s)) { // TODO test the tab
            Containers::StringView filename;
            const char* fileBegin = data.begin();
            while(in) {
                const Containers::StringView lineEnd = in.findOr('\n', in.end());
                // TODO document CR removal too
                const Containers::StringView line = in.prefix(lineEnd.begin())
                    .trimmedPrefix(" \t"_s)
                    .trimmedSuffix("\r"_s);
                in = in.suffix(lineEnd.end());

                // TODO test the tab after too!
                if(line.hasPrefix("0 FILE "_s) || line.hasPrefix("0 FILE\t"_s)) {
                    /* The first file shouldn't be referenced by anything else
                       so it doesn't make sense to insert it into the map */
                    // TODO the slicing is a mess, it should end at prev line end, not this line trimmed begin...
                    // TODO well this _does_ pick the first file also :/
                    if(filename) {
                        // TODO document the replacement
                        fileMap.emplace(Utility::String::replaceAll(filename, '\\', '/'), files.size());
                        // TODO why cannot i pass the string there
                        arrayAppend(files, InPlaceInit).contents = data.slice(fileBegin, line.begin());
                    }

                    if(!topLevelFile) // TODO make this an else to above, this is a messy hotfix, with else it's now an empty slice
                        topLevelFile = data.slice(fileBegin, line.begin());
                    filename = line.exceptPrefix(7).trimmed(" \t");
                    fileBegin = line.begin();
                }
            }

            // TODO goddamit deduplicate this, maybe do the filename replacement when parsing already?
            if(filename) {
                // TODO document the replacement
                fileMap.emplace(Utility::String::replaceAll(filename, '\\', '/'), files.size());
                // TODO why cannot i pass the string there
                arrayAppend(files, InPlaceInit).contents = data.suffix(fileBegin);
            }

        } else topLevelFile = in;
    }

    /* Recursively parse the top-level file line by line. If anything fails,
       bail. */
    const Containers::Optional<File> parsedTopLevelFile = parse(
        state->vertices,
        state->indices,
        state->meshes,
        files,
        fileMap,
        subFiles,
        {}, // TODO fill once we can open real files
        std::getenv("LDRAWDIR"), // TODO fail if not present, use a config value, cache this too
        {}, /* No path prefix for the top-level file */
        topLevelFile);
    if(!parsedTopLevelFile)
        return;

    /* Recursively go through the parsed files, where each file can be
       referenced multiple times, and create a scene tree */
    populateScene(
        colorMap,
        state->materials,
        files,
        subFiles,
        state->objects,
        state->objectMeshesMaterials,
        -1,
        Matrix4::scaling(Vector3{0.01f})*Matrix4::rotationX(180.0_degf), // TODO uh also the axsi flip
        16, // TODO er, "the default color", i guess; have a constant
        *parsedTopLevelFile);

    /* All done, move the state in */
    _state = Utility::move(state);
}

UnsignedInt LdrawImporter::doMeshCount() const {
    return _state->meshes.size();
}

Containers::Optional<MeshData> LdrawImporter::doMesh(const UnsignedInt id, UnsignedInt) {
    const State& state = *_state;
    const Mesh& mesh = state.meshes[id];

    /* Copy appropriate slice of the global vertex and index data */
    /** @todo once zero-copy import is a thing, just reference the memory
        instead */
    Containers::Array<char> vertexData{InPlaceInit, Containers::arrayCast<const char>(state.vertices.slice(mesh.vertexBegin, mesh.vertexEnd))};
    Containers::Array<char> indexData{InPlaceInit, Containers::arrayCast<const char>(state.indices.slice(mesh.indexBegin, mesh.indexEnd))};
    const Containers::StridedArrayView1D<Vertex> vertices = Containers::arrayCast<Vertex>(vertexData);
    const Containers::ArrayView<UnsignedInt> indices = Containers::arrayCast<UnsignedInt>(indexData);

    return Trade::MeshData{MeshPrimitive::Triangles,
        Utility::move(indexData), Trade::MeshIndexData{indices},
        Utility::move(vertexData), {
            Trade::MeshAttributeData{Trade::MeshAttribute::Position, vertices.slice(&Vertex::position)},
            Trade::MeshAttributeData{Trade::MeshAttribute::Normal, vertices.slice(&Vertex::normal)},
        }};
}

UnsignedInt LdrawImporter::doMaterialCount() const {
    return _state->materials.size();
}

Containers::Optional<MaterialData> LdrawImporter::doMaterial(const UnsignedInt id) {
    // TODO have a phong fallback toggle like GltfImporter
    return Trade::MaterialData{Trade::MaterialType::PbrMetallicRoughness|Trade::MaterialType::Phong, {
        {Trade::MaterialAttribute::BaseColor, _state->materials[id].color},
        {Trade::MaterialAttribute::DiffuseColor, _state->materials[id].color},
        // TODO important!!! document why (same meshes used for inside and outside faces)
        {Trade::MaterialAttribute::DoubleSided, true},
        // TODO this also
        {Trade::MaterialAttribute::SpecularColor, 0x00000000_rgbaf},
    }};
}

UnsignedLong LdrawImporter::doObjectCount() const {
    return _state->objects.size();
}

UnsignedInt LdrawImporter::doSceneCount() const {
    return 1;
}

Containers::Optional<SceneData> LdrawImporter::doScene(UnsignedInt) {
    const State& state = *_state;

    Containers::ArrayView<UnsignedInt> objectIds;
    Containers::StridedArrayView1D<Object> objects;
    Containers::StridedArrayView1D<Containers::Triple<UnsignedInt, UnsignedInt, Int>> objectMeshesMaterials;
    Containers::ArrayTuple data{
        {NoInit, state.objects.size(), objectIds},
        {NoInit, state.objects.size(), objects},
        {NoInit, state.objectMeshesMaterials.size(), objectMeshesMaterials},
    };

    /* Trivial mapping for everything in the Object struct */
    UnsignedInt id = 0;
    for(UnsignedInt& i: objectIds)
        i = id++;

    Utility::copy(state.objects, objects);
    Utility::copy(state.objectMeshesMaterials, objectMeshesMaterials);

    return Trade::SceneData{
        Trade::SceneMappingType::UnsignedInt, state.objects.size(), Utility::move(data), {
            Trade::SceneFieldData{Trade::SceneField::Parent,
                objectIds,
                objects.slice(&Object::parent)},
            Trade::SceneFieldData{Trade::SceneField::Transformation,
                objectIds,
                objects.slice(&Object::transformation)},
            Trade::SceneFieldData{Trade::SceneField::Mesh,
                objectMeshesMaterials.slice(&decltype(objectMeshesMaterials)::Type::first),
                objectMeshesMaterials.slice(&decltype(objectMeshesMaterials)::Type::second)},
            Trade::SceneFieldData{Trade::SceneField::MeshMaterial,
                objectMeshesMaterials.slice(&decltype(objectMeshesMaterials)::Type::first),
                objectMeshesMaterials.slice(&decltype(objectMeshesMaterials)::Type::third)},
        }};
}

Int LdrawImporter::doDefaultScene() const {
    return 0;
}

}}

CORRADE_PLUGIN_REGISTER(LdrawImporter, Magnum::Trade::LdrawImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
