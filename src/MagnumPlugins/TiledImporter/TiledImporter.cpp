/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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

#include "TiledImporter.h"
#include "TiledImporterData.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>

#include <sstream>
// dependencies
#include "base64.h"
#include "miniz.c"


namespace Magnum { namespace Trade {
    namespace xml_helpers {
        void queryAttribute(pugi::xml_attribute attr, std::string& result) {
            result = std::string(attr.as_string());
        }

        void queryAttribute(pugi::xml_attribute attr, Float& result) {
            result = attr.as_float();
        }

        void queryAttribute(pugi::xml_attribute attr, Int& result) {
            result = attr.as_int();
        }

        void queryAttribute(pugi::xml_attribute attr, UnsignedInt& result) {
            result = attr.as_uint();
        }

        void queryAttribute(pugi::xml_attribute attr, bool& result) {
            result = attr.as_bool();
        }

        void queryAttribute(pugi::xml_attribute attr, Color4& result) {
            auto colorStr = attr.as_string();
            char *end;
            auto colorUint = std::strtol(colorStr[0] == '#' ? colorStr + 1: colorStr, &end, 16);
            result.fromSrgbAlpha(colorUint);
        }

        // Parses the attribute tag in xml_element and saves it in ret_value. If error returns false, else returns true.
        template <typename T>
        bool parseAttribute(T& ret_value, const char* tag, pugi::xml_node node, bool is_optional = false) {
            if (!node) {               
                Warning{} << "Invalid pugi xml node while parsing tag: " << tag;
                return false;
            }

            auto attr = node.attribute(tag);
            if (!attr) {
                if (is_optional) {
                    return true;
                } else {
                    Warning{} << "Non-optional attribute not found, attribute tag: " << tag;
                    return false;
                }
            } else {
                queryAttribute(attr, ret_value);
                return true;
            }
        }

        // Parse the properties and fills the out_properties parameter. returns false on error, true on success.
        bool parseProperties(pugi::xml_node propertiesNode, TiledPropertiesMapData& outProperties) {
            bool noError = true;
            if (propertiesNode) {
                auto propertyNode = propertiesNode.child("property");
                while (propertyNode) {
                    std::string name, value;
                    bool attrFound = true;
                    attrFound &= xml_helpers::parseAttribute(name, "name", propertyNode);
                    attrFound &= xml_helpers::parseAttribute(value, "value", propertyNode);
                    if (attrFound) {
                        outProperties.PropertiesMap.emplace(name, value);
                    } else {
                        Warning{} << "Malformed property. Omitting it.";
                        noError = false;
                    }
                    propertyNode = propertyNode.next_sibling("property");
                }

            }
            return noError;
        }

        
        void checkDataEndiannes(Containers::ArrayView<UnsignedInt>& data) {
            // From tmx map format:
            // Now you have an array of bytes, which should be interpreted as an array of
            // unsigned 32-bit integers using little-endian byte ordering.
            for (UnsignedInt& i : data) Utility::Endianness::littleEndian(i);
        }


    } // xml_helpers namespace end

#define PARSE_CHECKED_OPTIONAL(Destination, Tag, Node)  if (xml_helpers::parseAttribute(Destination, Tag, Node, true) == false) { return; }
#define PARSE_CHECKED(Destination, Tag, Node)  if (xml_helpers::parseAttribute(Destination, Tag, Node, false) == false) { return; }
#define FILL_LAYER_TILES_FROM_GIDS_CHECKED(gids, tm, layer) if ( fillLayerTilesDataFromGIDs(gids, tm, layer) == false) {return;}
    
    void checkTMXVersion(pugi::xml_node node) {
        std::string version = "NONE";
        xml_helpers::parseAttribute(version, "version", node, true);
        if (version != "1.2") {
            Warning{} << "Tilemap saved with a tiled TMX format version: " <<
                version << ". This parser is only tested with TMX format version 1.2, errors may occur";
        }
    }

    void splitTileGIDAndTileFlagsFromUnsplittedGID(UnsignedInt unsplitted_gid, UnsignedInt& gid, bool& flipH, bool& flipV, bool& flipD) {
        // Split the gid into flip bits and gid tile index
        // more info: http://docs.mapeditor.org/en/stable/reference/tmx-map-format/#tile-flipping
        static const UnsignedInt FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
        static const UnsignedInt FLIPPED_VERTICALLY_FLAG = 0x40000000;
        static const UnsignedInt FLIPPED_DIAGONALLY_FLAG = 0x20000000;

        // Read the flipping flags first
        flipH = (unsplitted_gid & FLIPPED_HORIZONTALLY_FLAG) != 0;
        flipV = (unsplitted_gid & FLIPPED_VERTICALLY_FLAG) != 0;
        flipD = (unsplitted_gid & FLIPPED_DIAGONALLY_FLAG) != 0;

        gid = unsplitted_gid;
        gid &= ~(FLIPPED_HORIZONTALLY_FLAG | FLIPPED_VERTICALLY_FLAG | FLIPPED_DIAGONALLY_FLAG);
    }

    // Returns the tileset index  in the Tilesets array for a given GID. Returns -1 if no tileset found (error)
    bool getTilsetIdForGID(UnsignedInt GID, const std::vector<TiledTilesetData>& tilesets, UnsignedInt& outTilesetId) {
        outTilesetId = 0;
        for (auto& tileset : tilesets) {
            const auto last_gid = tileset.FirstGID + tileset.TileCount;
            if ((GID >= tileset.FirstGID) && (GID < last_gid)) {
                return true;
            }
            outTilesetId++;
        }
        return false;
    }

    // Fills FAPTiledTilemapDataLayer Tiles for a given array of GIDs. On Error return false 
    // ENSURE that gid is different from 0 BEFORE calling this function.
    // x and y are the coordinates in the layer for this tile
    bool fillTiledTileDataFromGID(UnsignedInt unsplitted_gid, const std::vector<TiledTilesetData>& tilesets, TiledTileData& tile, UnsignedInt x = 0, UnsignedInt y = 0) {
        UnsignedInt gid;
        splitTileGIDAndTileFlagsFromUnsplittedGID(unsplitted_gid, gid, tile.FlipH, tile.FlipV, tile.FlipD);

        // retrieve the tilset index for this valid gid
        UnsignedInt ts_idx;
        if (!getTilsetIdForGID(gid, tilesets, ts_idx)) {
            //TODO TILED_PARSER_WARNING(TEXT("GID %d not found in any Tileset of the Tilemap!!"), gid);
            return false;
        }

        auto& ts = tilesets[ts_idx];

        // Fill the tile layer data and insert it
        tile.TilesetId = ts_idx;
        tile.X = x;
        tile.Y = y;

        // Retrieve the tileset tile position in the TILESET
        tile.TileIndex = gid - ts.FirstGID;
        tile.TilesetX = tile.TileIndex % ts.TileColumns;
        tile.TilesetY = tile.TileIndex / ts.TileColumns;

        return true;
    }

    // Fills FAPTiledTilemapDataLayer Tiles for a given array of GIDs. On Error return false 
    void fillLayerTilesDataFromGIDs(const Containers::ArrayView<UnsignedInt>& gids, const TiledTilemapData& tm, TiledLayerData& layer) {
        for (std::size_t gid_idx = 0; gid_idx < gids.size(); gid_idx++) {
            const auto& gid = gids[gid_idx];
            // if 0 no tile is in this position, continue searching others
            if (gid == 0) {
                continue;
            }

            TiledTileData layer_tdata;
            // Retrieve the POSITION in the layer for this tile
            const auto x = gid_idx % tm.Width;
            const auto y = gid_idx / tm.Width;
            if (fillTiledTileDataFromGID(gid, tm.Tilesets, layer_tdata, x, y)) {
                layer.Tiles[{ layer_tdata.X, layer_tdata.Y }] = layer_tdata;
            }
        }
    }

    // Returns false on error.
    bool parsePointsObjectAttribute(pugi::xml_node object_el, std::vector<Vector2>& points) {
        std::string points_data;
        if (xml_helpers::parseAttribute(points_data, "points", object_el)) {
            std::stringstream ssPointsData(points_data);
            std::vector<Float> pointsVec;
            std::string substr;
            while (ssPointsData.good()) {
                std::getline(ssPointsData, substr, ' ');
                std::stringstream ssPointData(substr);
                while (ssPointData.good()) {
                    std::getline(ssPointData, substr, ',');
                    pointsVec.push_back(std::stof(substr));
                }
            }
            for (std::size_t pointIdx = 0; pointIdx < pointsVec.size(); pointIdx += 2) {
                points.push_back({pointsVec.at(pointIdx), pointsVec.at(pointIdx + 1)});
            }
        } else {
            Warning{} << "points attribute not found in Polygon object. Polygon object must have points attribute.";
            return false;
        }
        return true;
    }

    // Returns true if object is parsed succesfully or no object element to parse (which is ok)
    // Returns false if the object exists but there has been an error parsing it
    bool fillObjectFromObjectElement(pugi::xml_node object_el, TiledObjectData& obj_data, const std::vector<TiledTilesetData>& tilesets) {
        if (!object_el) {
            // no object not means an error.
            return true;
        }

        xml_helpers::parseAttribute(obj_data.InternalTiledObjectId, "id", object_el, true);
        xml_helpers::parseAttribute(obj_data.Name, "name", object_el, true);
        xml_helpers::parseAttribute(obj_data.Type, "type", object_el, true);
        xml_helpers::parseAttribute(obj_data.X, "x", object_el, true);
        xml_helpers::parseAttribute(obj_data.Y, "y", object_el, true);
        xml_helpers::parseAttribute(obj_data.Height, "height", object_el, true);
        xml_helpers::parseAttribute(obj_data.Width, "width", object_el, true);
        xml_helpers::parseAttribute(obj_data.Rotation, "rotation", object_el, true);
        xml_helpers::parseAttribute(obj_data.IsVisible, "visible", object_el, true);

        // Retrieve the properties map for this object
        xml_helpers::parseProperties(object_el.child("properties"), obj_data.Properties);

        // now retrieve the object type
        UnsignedInt unsplitted_gid;
        if (object_el.attribute("gid")) {
            xml_helpers::parseAttribute(unsplitted_gid, "gid", object_el);
            // Tile information found, object type Tile
            obj_data.ObjectType = EAPTiledObjectType::Tile;
            if (unsplitted_gid != 0) {
                return fillTiledTileDataFromGID(unsplitted_gid, tilesets, obj_data.Tile);
            } else {
                Warning{} << "Error: GID of zero in a tileset object parsing. Object name: " << obj_data.Name << " Ommiting this object.";
                return false;
            }
        } else if (object_el.child("point")) {
            obj_data.ObjectType = EAPTiledObjectType::Point;
        } else if (object_el.child("ellipse")) {
            obj_data.ObjectType = EAPTiledObjectType::Ellipse;
        } else if (auto polyline_el = object_el.child("polyline")) {
            obj_data.ObjectType = EAPTiledObjectType::Polyline;
            if (!parsePointsObjectAttribute(polyline_el, obj_data.Points)) {
                Warning{} << "Object name: " << obj_data.Name << " Ommiting this object.";
                return false;
            }
            return true;
        } else if (auto polygon_el = object_el.child("polygon")) {
            obj_data.ObjectType = EAPTiledObjectType::Polygon;
            if (!parsePointsObjectAttribute(polygon_el, obj_data.Points)) {
                Warning{} << "Object name: " << obj_data.Name << " Ommiting this object.";
                return false;
            }
            return true;
        } else if (auto text_type_el = object_el.child("text")) {
            obj_data.ObjectType = EAPTiledObjectType::Text;

            auto text = text_type_el.text();
            if (text) {
                obj_data.Text.Text = text.get();
            }

            xml_helpers::parseAttribute(obj_data.Text.FontFamily, "fontfamily", text_type_el, true);
            xml_helpers::parseAttribute(obj_data.Text.FontSize, "pixelsize", text_type_el, true);
            xml_helpers::parseAttribute(obj_data.Text.Color, "color", text_type_el, true);
            xml_helpers::parseAttribute(obj_data.Text.Bold, "bold", text_type_el, true);
            xml_helpers::parseAttribute(obj_data.Text.Italic, "italic", text_type_el, true);
            xml_helpers::parseAttribute(obj_data.Text.Underline, "underline", text_type_el, true);
            xml_helpers::parseAttribute(obj_data.Text.Strikeout, "strikeout", text_type_el, true);
            xml_helpers::parseAttribute(obj_data.Text.Kerning, "kerning", text_type_el, true);

            std::string halign;
            xml_helpers::parseAttribute(halign, "halign", text_type_el, true);
            if (halign == "center") {
                obj_data.Text.HAlign = EAPTiledTextHAlign::Center;
            } else if (halign == "right") {
                obj_data.Text.HAlign = EAPTiledTextHAlign::Right;
            } else if (halign == "justify") {
                obj_data.Text.HAlign = EAPTiledTextHAlign::Justify;
            }

            std::string valign;
            xml_helpers::parseAttribute(valign, "valign", text_type_el, true);
            if (valign == "center") {
                obj_data.Text.VAlign = EAPTiledTextVAlign::Center;
            } else if (valign == "bottom") {
                obj_data.Text.VAlign = EAPTiledTextVAlign::Bottom;
            }

        } else if (true) { // no child means rectangle
            obj_data.ObjectType = EAPTiledObjectType::Rectangle;
        }


        return true;
    }

    // Fills an array of objects from objectgroup element if it's valid
    // Returns true if objectgroup is parsed succesfully or no objectgroup element to parse (which is ok)
    // Returns false if the objectgroup exists but there has been an error parsing it
    bool fillObjectsFromObjectGroupElement(const std::string& OwnerFile, pugi::xml_node objectgroup_el, std::vector<TiledObjectData>& objs_array, const std::vector<TiledTilesetData>& tilesets) {
        // No object group found
        if (!objectgroup_el) {
            return true;
        }
        bool parsing_ok = true;

        for (auto object_el = objectgroup_el.child("object"); object_el; object_el = object_el.next_sibling("object")) {
            TiledObjectData obj_data;

            // check first if this object is a template or not
            std::string template_file;
            if (object_el.attribute("template")) {
                xml_helpers::parseAttribute(template_file, "template", object_el, false);
                // if this object is part of a template load the file and fill object
                pugi::xml_document template_xml_doc;
                const auto pathNative = Utility::Directory::path(Utility::Directory::fromNativeSeparators(OwnerFile));
                template_file = Utility::Directory::join(pathNative, template_file);
                auto result = template_xml_doc.load_file(template_file.c_str());
                if (result) {
                    // We have a template, then first fill the template information
                    auto object_template_el = template_xml_doc.first_child().child("object");
                    auto no_error = fillObjectFromObjectElement(object_template_el, obj_data, tilesets);
                    if (!no_error) {
                        parsing_ok = false;
                        Warning{} << "Error parsing object from template file. Omitting this object. File: "<< template_file;
                    } else {
                        // save the object type
                        const auto obj_type = obj_data.ObjectType;
                        // we loaded the template info, now load the concrete object info (if any parameter is modified from template like X or Y etc..)
                        no_error = fillObjectFromObjectElement(object_el, obj_data, tilesets);
                        if (!no_error) {
                            parsing_ok = false;
                            Warning{} << "Error parsing object from template file. Omitting this object. File: " << template_file;
                        } else {
                            // set the template object type in case it changed.
                            obj_data.ObjectType = obj_type;
                            objs_array.push_back(obj_data);
                        }
                    }
                } else { // template file not found, omit this object
                    parsing_ok = false;
                    Warning{} << "Object template file not found: "<< template_file<<". Omitting this object.";
                }
            } else {
                // Object is not part of a template
                if (fillObjectFromObjectElement(object_el, obj_data, tilesets) == false) {
                    parsing_ok = false;
                    Warning{} << "Error parsing object. Omitting this object. ";
                } else {
                    objs_array.push_back(obj_data);
                }
            }
        }
        return parsing_ok;
    }

    // Fills an FAPTiledObjectDataGroupData from objectgroup element if it's valid
    // Returns true if objectgroup is parsed succesfully or no objectgroup element to parse (which is ok)
    // Returns false if the objectgroup exists but there has been an error parsing it
    bool fillObjectGroupFromObjectGroupElement(const std::string& OwnerFilePath, pugi::xml_node objectgroup_el, TiledObjectDataGroupData& objg, const std::vector<TiledTilesetData>& tilesets) {
        // No object group found
        if (!objectgroup_el) {
            return true;
        }
        xml_helpers::parseAttribute(objg.Color, "color", objectgroup_el, true);
        return fillObjectsFromObjectGroupElement(OwnerFilePath, objectgroup_el, objg.Objects, tilesets);
    }


    // pugixml reads spaces endlines etc.. from text.. we have to erase that to get the data correctly.
    void sanitizeTextFromData(std::string& inOutText) {
        inOutText.erase(std::remove(inOutText.begin(), inOutText.end(), ' '), inOutText.end());
        inOutText.erase(std::remove(inOutText.begin(), inOutText.end(), '\n'), inOutText.end());
        inOutText.erase(std::remove(inOutText.begin(), inOutText.end(), '\r'), inOutText.end());
    }



    void parseTiledTilesetXMLNode(const std::string& tilesetFile, pugi::xml_node tilesetNode, bool& IsValid, TiledTilesetData& TiledTileset, const std::vector<TiledTilesetData>& tilesets) {
        IsValid = false;
        PARSE_CHECKED(TiledTileset.Name, "name", tilesetNode);
        PARSE_CHECKED(TiledTileset.TileWidth, "tilewidth", tilesetNode);
        PARSE_CHECKED(TiledTileset.TileHeight, "tileheight", tilesetNode);
        PARSE_CHECKED(TiledTileset.TileCount, "tilecount", tilesetNode);
        PARSE_CHECKED(TiledTileset.TileColumns, "columns", tilesetNode);
        TiledTileset.TileRows = TiledTileset.TileCount / TiledTileset.TileColumns;

        PARSE_CHECKED_OPTIONAL(TiledTileset.Spacing, "spacing", tilesetNode);
        PARSE_CHECKED_OPTIONAL(TiledTileset.Margin, "margin", tilesetNode);


        // parse tileset properties element
        xml_helpers::parseProperties(tilesetNode.child("properties"), TiledTileset.Properties);

        // Parse tileset tileoffset element
        auto tileset_tileoffset_el = tilesetNode.child("tileoffset");
        if (tileset_tileoffset_el) {
            PARSE_CHECKED_OPTIONAL(TiledTileset.TileOffsetX, "x", tileset_tileoffset_el);
            PARSE_CHECKED_OPTIONAL(TiledTileset.TileOffsetY, "y", tileset_tileoffset_el);
        }

        // Parse tileset image element
        auto tileset_image_el = tilesetNode.child("image");
        if (tileset_image_el) {
            PARSE_CHECKED(TiledTileset.ImagePath, "source", tileset_image_el);
            PARSE_CHECKED(TiledTileset.ImageWidth, "width", tileset_image_el);
            PARSE_CHECKED(TiledTileset.ImageHeight, "height", tileset_image_el);
        }

        // Parse tileset tile elements
        for (auto tileset_tile_el = tilesetNode.child("tile"); tileset_tile_el; tileset_tile_el = tileset_tile_el.next_sibling("tile")) {
            TiledTilesetTileData tile_data;
            UnsignedInt tile_local_id;
            PARSE_CHECKED(tile_local_id, "id", tileset_tile_el);

            // parse per tile properties
            if (!xml_helpers::parseProperties(tileset_tile_el.child("properties"), tile_data.Properties)) {
                Warning{} << "Malformed property. Omitting it.";
            }

            // parse per tile objects 
            auto temp_tilesets = tilesets;
            temp_tilesets.push_back(TiledTileset);
            if (!fillObjectsFromObjectGroupElement(tilesetFile, tileset_tile_el.child("objectgroup"), tile_data.Objects, temp_tilesets)) {
                Warning{} << "In tile id: " << tile_local_id;
            }
            TiledTileset.PerTileData[tile_local_id] = tile_data;
        }
        // parsing ok
        IsValid = true;
    }

    void parseTiledTilesetFile(const std::string& fullFilePath, bool& IsValid, TiledTilesetData& tiledTileset) {
        IsValid = false;
        pugi::xml_document xml_doc;
        pugi::xml_parse_result result = xml_doc.load_file(fullFilePath.c_str());
        if (!result) {
            Error{} << "Can't open tileset file: "<< fullFilePath <<". error: "<< result.description();
        } else {
            auto tsNode = xml_doc.first_child();
            checkTMXVersion(tsNode);
            parseTiledTilesetXMLNode(fullFilePath, tsNode, IsValid, tiledTileset, {});
        }
    }

    void parseTiledTilemapFile(const pugi::xml_document& xml_doc, bool& isValid, TiledTilemapData& TiledTileMapData, const std::string& currentFilename) {
        (void)TiledTileMapData;
        isValid = false;

        auto mapNode = xml_doc.child("map");
        checkTMXVersion(mapNode);
        

        // First CHECK for unsupported things
        Int infinite = 0;
        auto infHandler = mapNode.attribute("infinite");
        xml_helpers::parseAttribute(infinite, "infinite", mapNode);

        if (infinite != 0) {
            //TODO TILED_PARSER_WARNING(TEXT("Infinite TileMaps are not supported"));
            return;
        }

        // parse orientation
        std::string orientation;
        xml_helpers::parseAttribute(orientation, "orientation", mapNode);
        if (orientation == "isometric") {
            TiledTileMapData.Orientation = EAPTiledTileMapOrientation::Isometric;
        } else if (orientation == "orthogonal") {
            TiledTileMapData.Orientation = EAPTiledTileMapOrientation::Orthogonal;
        } else if (orientation == "staggered") {
            TiledTileMapData.Orientation = EAPTiledTileMapOrientation::IsometricStaggered;
        } else if (orientation == "hexagonal") {
            TiledTileMapData.Orientation = EAPTiledTileMapOrientation::Hexagonal;
            xml_helpers::parseAttribute(TiledTileMapData.HexSideLength, "hexsidelength", mapNode, true);
        } else {
            Warning{} << "Orthogonal isometric and hexagonal tiled maps are supported. Type: " << orientation << " not supported";
            return;
        }

        if (TiledTileMapData.Orientation == EAPTiledTileMapOrientation::Hexagonal || TiledTileMapData.Orientation == EAPTiledTileMapOrientation::IsometricStaggered) {
            std::string staggerIndex;
            if (xml_helpers::parseAttribute(staggerIndex, "staggerindex", mapNode, true)) {
                if (staggerIndex == "even") {
                    TiledTileMapData.StaggeringIndex = EAPTiledStaggeringIndex::Even;
                } else {
                    TiledTileMapData.StaggeringIndex = EAPTiledStaggeringIndex::Odd;
                }
            }

            std::string staggerAxis;
            if (xml_helpers::parseAttribute(staggerAxis, "staggeraxis", mapNode, true)) {
                if (staggerAxis == "x") {
                    TiledTileMapData.StaggeringAxis = EAPTiledStaggeringAxis::X;
                } else {
                    TiledTileMapData.StaggeringAxis = EAPTiledStaggeringAxis::Y;
                }
            }
        }

        // parse render order
        std::string render_order;
        PARSE_CHECKED(render_order, "renderorder", mapNode);
        if (render_order == "right-down") {
            TiledTileMapData.RenderOrder = EAPTiledRenderOrder::RightDown;
        } else if (render_order == "right-up") {
            TiledTileMapData.RenderOrder = EAPTiledRenderOrder::RightUp;
        } else if (render_order == "left-down") {
            TiledTileMapData.RenderOrder = EAPTiledRenderOrder::LeftDown;
        } else if (render_order == "left-up") {
            TiledTileMapData.RenderOrder = EAPTiledRenderOrder::LeftUp;
        }


        //// Now check necessary data
        PARSE_CHECKED(TiledTileMapData.TileWidth, "tilewidth", mapNode);
        PARSE_CHECKED(TiledTileMapData.TileHeight, "tileheight", mapNode);
        PARSE_CHECKED(TiledTileMapData.Width, "width", mapNode);
        PARSE_CHECKED(TiledTileMapData.Height, "height", mapNode);

        //// Parse map properties element
        xml_helpers::parseProperties(mapNode.child("properties"), TiledTileMapData.Properties);


        // Parse map tileset elements
        {
            auto mapTilesetNode = mapNode.child("tileset");
            while (mapTilesetNode) {
                // Parse map tileset node
                bool isTilesetValid = false;
                const auto currPath = Utility::Directory::path(Utility::Directory::fromNativeSeparators(currentFilename));

                TiledTilesetData tilesetData;
                // First check if the tilemap is embedded or is in another file
                std::string tilesetFile;
                if (xml_helpers::parseAttribute(tilesetFile, "source", mapTilesetNode, true)) {
                    // Is in another file
                    const auto tsFile = Utility::Directory::join(currPath, tilesetFile);
                    parseTiledTilesetFile(tsFile, isTilesetValid, tilesetData);
                } else {
                    // Is embedded
                    parseTiledTilesetXMLNode(currPath, mapTilesetNode, isTilesetValid, tilesetData, TiledTileMapData.Tilesets);
                }

                if (!isTilesetValid) {
                    //TODO return;
                } else {
                    PARSE_CHECKED(tilesetData.FirstGID, "firstgid", mapTilesetNode);
                    TiledTileMapData.Tilesets.emplace_back(tilesetData);
                }

                mapTilesetNode = mapTilesetNode.next_sibling("tileset");
            }
        }



        // Parse tile layer properties elements
        {
            
            for (auto mapLayerNode = mapNode.child("layer"); mapLayerNode; mapLayerNode = mapLayerNode.next_sibling("layer")) {
                TiledLayerData layerData;

                PARSE_CHECKED(layerData.Name, "name", mapLayerNode);
                PARSE_CHECKED_OPTIONAL(layerData.Opacity, "opacity", mapLayerNode);
                PARSE_CHECKED_OPTIONAL(layerData.IsVisible, "visible", mapLayerNode);
                PARSE_CHECKED_OPTIONAL(layerData.OffsetX, "offsetx", mapLayerNode);
                PARSE_CHECKED_OPTIONAL(layerData.OffsetY, "offsety", mapLayerNode);

                // Parse layer properties element
                xml_helpers::parseProperties(mapLayerNode.child("properties"), layerData.Properties);

                auto layerDataNode = mapLayerNode.child("data");
                if (layerDataNode) {

                    std::string layerDataEncoding;
                    PARSE_CHECKED_OPTIONAL(layerDataEncoding, "encoding", layerDataNode);

                    if (layerDataEncoding == "base64") {
                        // BASE64 encoding
                        std::string dataToDecode = layerDataNode.text().get();
                        sanitizeTextFromData(dataToDecode);
                        std::string dataDecoded = base64_decode(dataToDecode);

                        // now uncompress if necessary
                        std::string layerDataCompression;
                        PARSE_CHECKED_OPTIONAL(layerDataCompression, "compression", layerDataNode);
                        

                        if (layerDataCompression == "") {
                            // BASE64 with no compression 
                            Containers::ArrayView<UnsignedInt> dataGIds{(UnsignedInt *)dataDecoded.data() , dataDecoded.size() / 4 };
                            xml_helpers::checkDataEndiannes(dataGIds);
                            fillLayerTilesDataFromGIDs(dataGIds, TiledTileMapData, layerData);
                            // Add it to the layers
                            TiledTileMapData.Layers.push_back(layerData);
                        } else {
                            // BASE64 with compression
                            if (layerDataCompression == "zlib") {
                                uLongf outLen = TiledTileMapData.Width * TiledTileMapData.Height * 4; // uint are 4 bytes so we multiply by four
                                UnsignedInt *out = (UnsignedInt *)malloc(outLen);
                                Containers::ScopeGuard outDataGuard{ out, free };
                                uncompress((Bytef *)out, &outLen,(const Bytef*)dataDecoded.c_str(), dataDecoded.size());
                                Containers::ArrayView<UnsignedInt> dataGIds{ out , outLen / 4};
                                xml_helpers::checkDataEndiannes(dataGIds);
                                fillLayerTilesDataFromGIDs(dataGIds, TiledTileMapData, layerData);
                                // Add it to the layers
                                TiledTileMapData.Layers.push_back(layerData);
                            } else if (layerDataCompression == "gzip") {
                                Warning{} << "Tile layer format (GZIP) is not supported and it's deprecated by Tiled. Use Base64/zlib compression in Tiled. Omitting this layer";
                            } else {
                                Warning{} << "Tile layer format "<< layerDataCompression<<" is not supported. Use Base64/zlib compression in Tiled. Omitting this layer";
                            }
                        }
                    } else if (layerDataEncoding == "csv") {
                        // CSV encoding
                        std::string csvData = layerDataNode.text().get();
                        sanitizeTextFromData(csvData);
                        std::stringstream ssCSVData(csvData);
                        std::vector<UnsignedInt> tilesGids;
                        std::string substr;
                        while (ssCSVData.good()) {
                            std::getline(ssCSVData, substr, ',');
                            tilesGids.push_back(std::stoul(substr));
                        }
                        Containers::ArrayView<UnsignedInt> view(tilesGids.data(), tilesGids.size());
                        fillLayerTilesDataFromGIDs(view, TiledTileMapData, layerData);
                        TiledTileMapData.Layers.push_back(layerData);
                    } else {
                        // XML encoding
                        std::vector<UnsignedInt> tilesGids;
                        for (auto tileDataNode = layerDataNode.child("tile");  tileDataNode; tileDataNode = tileDataNode.next_sibling("tile")) {
                            UnsignedInt tile_gid = 0;
                            xml_helpers::parseAttribute(tile_gid, "gid", tileDataNode, false);
                            tilesGids.push_back(tile_gid);
                        }
                        Containers::ArrayView<UnsignedInt> view(tilesGids.data(), tilesGids.size());
                        fillLayerTilesDataFromGIDs(view, TiledTileMapData, layerData);
                        TiledTileMapData.Layers.push_back(layerData);
                    }
                }
            }
            // When layers are parsed, reverse the Array.
            std::reverse(TiledTileMapData.Layers.begin(), TiledTileMapData.Layers.end());
        }


        // Parse Image Layer properties elements
        {
            auto img_layer_el = mapNode.child("imagelayer");
            while (img_layer_el != nullptr) {
                TiledLayerData layer_data;

                layer_data.Type = EAPTiledLayerType::Image;
                PARSE_CHECKED(layer_data.Name, "name", img_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.Opacity, "opacity", img_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.IsVisible, "visible", img_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.OffsetX, "offsetx", img_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.OffsetY, "offsety", img_layer_el);

                // Parse layer properties element
                xml_helpers::parseProperties(img_layer_el.child("properties"), layer_data.Properties);

                auto img_el = img_layer_el.child("image");
                if (img_el) {
                    PARSE_CHECKED(layer_data.ImgFile, "source", img_el);
                    PARSE_CHECKED(layer_data.ImgSizeX, "width", img_el);
                    PARSE_CHECKED(layer_data.ImgSizeY, "height", img_el);
                    PARSE_CHECKED(layer_data.ImgTransColor, "trans", img_el);
                } // It's completely valid an image layer without image information.

                TiledTileMapData.Layers.push_back(layer_data);

                img_layer_el = img_layer_el.next_sibling("imagelayer");
            }
        }


        // Parse Object Layer properties elements
        {
            auto objg_layer_el = mapNode.child("objectgroup");
            while (objg_layer_el != nullptr) {
                TiledLayerData layer_data;

                layer_data.Type = EAPTiledLayerType::Object;
                PARSE_CHECKED(layer_data.Name, "name", objg_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.Opacity, "opacity", objg_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.IsVisible, "visible", objg_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.OffsetX, "offsetx", objg_layer_el);
                PARSE_CHECKED_OPTIONAL(layer_data.OffsetY, "offsety", objg_layer_el);

                // Parse layer properties element
                xml_helpers::parseProperties(objg_layer_el.child("properties"), layer_data.Properties);

                // parse object group
                fillObjectGroupFromObjectGroupElement(currentFilename, objg_layer_el, layer_data.ObjectGroup, TiledTileMapData.Tilesets);


                TiledTileMapData.Layers.push_back(layer_data);

                objg_layer_el = objg_layer_el.next_sibling("objectgroup");
            }
        }

        // Parsing ok set to true and return
        isValid = true;


    }



    TiledImporter::TiledImporter() {};
    TiledImporter::~TiledImporter() {};
    TiledImporter::TiledImporter(PluginManager::AbstractManager& manager, const std::string& plugin) : AbstractImporter{ manager, plugin } {}

    auto TiledImporter::doFeatures() const -> Features { return Feature::OpenData; }

    bool TiledImporter::doIsOpened() const {
        return bool(_data);
    }

    void TiledImporter::doClose() {

    }
    void TiledImporter::doOpenFile(const std::string& filename) {
        _currentFilename = filename;
        AbstractImporter::doOpenFile(filename);
    }

    void TiledImporter::doOpenData(Containers::ArrayView<const char> data) {
        _doc.reset();
        pugi::xml_parse_result result = _doc.load_buffer(data.data(), data.size());
        
        if (!result) {
            Error{} << "Trade::TiledImporter::openData(): error opening file:" << result.description();
            _data.reset();
            _doc.reset();
            doClose();
            return;
         
        } else {
            _data.reset(new TiledTilemapData());
            bool isValid = false;
            parseTiledTilemapFile(_doc, isValid, *_data, _currentFilename);
        }

        

    }









   

    










}}

CORRADE_PLUGIN_REGISTER(TiledImporter, Magnum::Trade::TiledImporter,"cz.mosra.magnum.Trade.AbstractImporter/0.3")


