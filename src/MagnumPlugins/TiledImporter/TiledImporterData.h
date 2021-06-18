#pragma once
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <map>
#include <vector> // TODO change to Array magnum

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_TILEDIMPORTER_BUILD_STATIC
#ifdef TiledImporter_EXPORTS
#define MAGNUM_TILEDIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
#else
#define MAGNUM_TILEDIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
#endif
#else
#define MAGNUM_TILEDIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_TILEDIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_TILEDIMPORTER_EXPORT
#define MAGNUM_TILEDIMPORTER_LOCAL
#endif

namespace Magnum {
    namespace Trade {
        /**
        * Holds the properties key-value map
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledPropertiesMapData {
            // User specified Key-Value pairs map
            std::map<std::string, std::string> PropertiesMap;

            // Returns true if no property is in the map
            bool empty() const {
                return PropertiesMap.size() == 0;
            }
            //// Returns the T value of the Key property if it's found
            //template <typename T>
            //Containers::Optional<T> getValue(const std::string& Key) const {
            //    auto* Value = PropertiesMap.fin(Key);
            //    return Value ? *Value : Containers::Optional<T>{};
            //}
        };

        //// Returns the Int value of the Key property if it's found
        //template <>
        //inline Containers::Optional<Int> TiledPropertiesMapData::GetValue(const std::string& Key) const {
        //    auto* Value = PropertiesMap.Find(Key);
        //    return Value ? FCString::Atoi(**Value) : Containers::Optional<Int>{};
        //}

        //// Returns the float value of the Key property if it's found
        //template <>
        //inline Containers::Optional<float> TiledPropertiesMapData::GetValue(const std::string& Key) const {
        //    auto* Value = PropertiesMap.Find(Key);
        //    return Value ? FCString::Atof(**Value) : Containers::Optional<float>{};
        //}

        //// Returns the bool value of the Key property if it's found
        //template <>
        //inline Containers::Optional<bool> TiledPropertiesMapData::GetValue(const std::string& Key) const {
        //    auto* Value = PropertiesMap.Find(Key);
        //    return Value ? Value->ToBool() : Containers::Optional<bool>{};
        //}

        //// Returns the Color4 value of the Key property if it's found
        //template <>
        //inline Containers::Optional<Color4> TiledPropertiesMapData::GetValue(const std::string& Key) const {
        //    auto* Value = PropertiesMap.Find(Key);
        //    return Value ? Color4::FromHex(*Value) : Containers::Optional<Color4>{};
        //}

        /**
        * Holds a layer Tile data
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledTileData {
        public:
            // Column where this tile is in the layer
            UnsignedInt X = 0;
            // Row where this tile is in the layer
            UnsignedInt Y = 0;
            // Tileset index in the Tilemap Tilesets array that this tile refers 
            UnsignedInt TilesetId = 0;
            // Index in the tileset that this tile refers.
            UnsignedInt TileIndex = 0;
            // Tile Column in the Tileset that this Tile refers.
            UnsignedInt TilesetX = 0;
            // Tile Row in the Tileset that this Tile refers.
            UnsignedInt TilesetY = 0;
            // Is Tile flipped horizontally
            bool FlipH = false;
            // Is Tile flipped vertically
            bool FlipV = false;
            // Is Tile flipped diagonally
            bool FlipD = false;

        };

        /**
        * Text horizontal alignment type
        */
        enum class EAPTiledTextHAlign {
            Left = 0,
            Center,
            Right,
            Justify
        };

        /**
        * Text vertical alignment type
        */
        enum class EAPTiledTextVAlign {
            Top = 0,
            Center,
            Bottom
        };

        /**
        * Holds the attributes of a Text object in Tiled
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledObjectDataTextParamsData {
            // Text of the text object
            std::string Text;

            // Font Family (sans-serif default)
            std::string FontFamily = "sans-serif";

            // Text font size
            Int FontSize;

            // Color of the text
            Color4 Color{ 0,0,0,1 };

            // Whether the font is bold
            bool Bold = false;

            // Whether the font is italic
            bool Italic = false;

            // Whether a line should be drawn below the text
            bool Underline = false;

            // Whether a line should be drawn through the text
            bool Strikeout = false;

            // Whether kerning should be used while rendering the text
            bool Kerning = false;

            // Horizontal alignment of the text within the object
            EAPTiledTextHAlign HAlign = EAPTiledTextHAlign::Left;

            // Vertical alignment of the text within the object
            EAPTiledTextVAlign VAlign = EAPTiledTextVAlign::Top;
        };

        // Type enumeration for Tiled Objects
        enum class EAPTiledObjectType {
            Rectangle = 0,
            Ellipse,
            Polygon,
            Polyline,
            Point,
            Text,
            Tile
        };

        /**
        * Holds the attributes of an object in Tiled
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledObjectData {
            // The Object type
            EAPTiledObjectType ObjectType;

            // The name of the object. User custom type
            std::string Name;

            // The type of the object. User custom type
            std::string Type;

            // The x position of the object in pixels
            float X = 0.f;

            // The y position of the object in pixels
            float Y = 0.f;

            // The width of the object in pixels
            float Width = 0.f;

            // The height of the object in pixels
            float Height = 0.f;

            // The rotation of the object in degrees clockwise
            float Rotation = 0.0f;

            // Whether the object is shown(true) or hidden(false)..
            bool IsVisible = true;

            // Tile data (valid only for Tile object type)
            TiledTileData Tile;

            // Points (valid only for Polygon and Polyline object type)
            std::vector<Vector2> Points;

            // Text data (valid only for Text object type)
            TiledObjectDataTextParamsData Text;

            // Object Properties map
            TiledPropertiesMapData Properties;

            // Object Identifier in Tiled (only used inside Tiled to identify the objects)
            Int InternalTiledObjectId;
        };

        /**
        * Holds the attributes of an objectgroup in Tiled
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledObjectDataGroupData {
            // Color of the object group
            Color4 Color{ 0,0,0,1 };

            // Objects of in this object group
            std::vector<TiledObjectData> Objects;
        };

        /**
        * Holds the properties of a Tile in the tileset file.
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledTilesetTileData {
            // Objects that this tile has
            std::vector<TiledObjectData> Objects;

            // User specified Key-Value pairs
            TiledPropertiesMapData Properties;
        };

        /**
        * Holds the parsed data from a tiled tileset file.
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledTilesetData {
            // Indicates the first Global Tile Identifier in a tilemap only valid if we are parsing an entire Tiled Tilemap.
            UnsignedInt FirstGID = 0;

            // Name of the tileset
            std::string Name;

            // The (maximum) width of the tiles in this tileset.
            UnsignedInt TileWidth = 0;

            // The (maximum) height of the tiles in this tileset.
            UnsignedInt TileHeight = 0;

            // The number of tile rows in this tileset
            UnsignedInt TileRows = 0;

            // The number of tile columns in this tileset
            UnsignedInt TileColumns = 0;

            // The number of tiles in this tileset
            UnsignedInt TileCount = 0;

            // The spacing to ignore around the outer edge of the source image (in pixels)
            UnsignedInt Margin = 0;

            // The spacing between each tile in the source image (in pixels)
            UnsignedInt Spacing = 0;

            // Offset in X (Positive is right) applied when drawing a tile from the related tileset (in pixels)
            Int TileOffsetX = 0;

            // Offset in Y (Positive is down) applied when drawing a tile from the related tileset (in pixels)
            Int TileOffsetY = 0;

            // Source image path
            std::string ImagePath;

            // Source image width dimension in pixels
            UnsignedInt ImageWidth = 0;

            // Source image height dimension in pixels
            UnsignedInt ImageHeight = 0;

            // Per-tile information (key is the local id of the tile in the tileset not the GID)
            std::map<UnsignedInt, TiledTilesetTileData> PerTileData;

            // User specified Key-Value pairs
            TiledPropertiesMapData Properties;

            // Returns the Tile Data by the given tile index in this tileset if it exists.
            Containers::Optional<const TiledTilesetTileData *> GetTileDataByTileIndex(Int TileIndex) const {
                auto TileData = PerTileData.find(TileIndex);
                return TileData != PerTileData.end() ? Containers::Optional<const TiledTilesetTileData *>{&TileData->second} : Containers::Optional<const TiledTilesetTileData *>();
            }

            // Returns an array with the tile indices that contains all of the properties passed by parameter
            std::vector<Int> GetTileIndicesWithProperties(const std::vector<std::string>& PropertiesArray) const {
                std::vector<Int> Result;
                for (auto& TileData : PerTileData) {
                    if (!TileData.second.Properties.empty()) {
                        bool AllFound = true;
                        for (auto& Property : PropertiesArray) {
                            if (TileData.second.Properties.PropertiesMap.find(Property) == TileData.second.Properties.PropertiesMap.end()) {
                                AllFound = false;
                                break;
                            }
                        }
                        if (AllFound) {
                            Result.push_back(TileData.first);
                        }
                    }
                }
                return Result;
            }

            // Returns an array with the tile indices that match all of the properties and their values passed by parameter (both vectors have to have the same size)
          /*  std::vector<Int> GetTileIndicesWithPropertiesValues(const std::vector<std::string>& PropertiesArray, const std::vector<std::string>& Values) const {
                if (PropertiesArray.size() != Values.size()) {
                    return {};
                }

                std::vector<Int> Result;
                for (auto& TileData : PerTileData) {
                    if (!TileData.second.Properties.empty()) {
                        bool AllFound = true;
                        for (auto IndexProperty = 0; IndexProperty < PropertiesArray.size(); IndexProperty++) {
                            auto& ValueToSearch = Values[IndexProperty];
                            auto& PropertyToSearch = PropertiesArray[IndexProperty];
                            auto ValueFound = TileData.second.Properties.GetValue<std::string>(PropertyToSearch);
                            if ((ValueFound && ValueFound.GetValue() == ValueToSearch) == false) {
                                AllFound = false;
                                break;
                            }
                        }
                        if (AllFound) {
                            Result.push_back(TileData.first);
                        }
                    }
                }
                return Result;
            }*/
        };

        // Type enumeration for Layer types
        enum class EAPTiledLayerType {
            Tile = 0,
            Image,
            Object, // Not implemented yet
        };

        /**
        * Holds the parsed data from a tiled tilemap layer.
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledLayerData {
            // Type of the layer
            EAPTiledLayerType Type = EAPTiledLayerType::Tile;

            // Name of the layer
            std::string Name;

            // Tiles in this layer (Only valid when LayerType is Tile)
            std::map<std::pair<UnsignedInt, UnsignedInt>, TiledTileData> Tiles;

            // Opacity of the layer. from 0 to 1. 1 Means full opaque, 0 fully transparent
            Float Opacity = 1.0f;

            // Is this layer visible?
            bool IsVisible = true;

            // Rendering offset in X for this layer in pixels
            float OffsetX = 0;

            // Rendering offset in Y for this layer in pixels
            float OffsetY = 0;

            // File used as the image in a image layer (Only valid when LayerType is Image)
            std::string ImgFile;

            // Image size X in pixels (Only valid when LayerType is Image)
            UnsignedInt ImgSizeX = 0;

            // Image size Y in pixels (Only valid when LayerType is Image)
            UnsignedInt ImgSizeY = 0;

            // Image Transparent Color (Only valid when LayerType is Image)
            Color4 ImgTransColor{ 0,0,0,1 };

            // Objectgroup (Only valid when LayerType is Object)
            TiledObjectDataGroupData ObjectGroup;

            // User specified Key-Value pairs
            TiledPropertiesMapData Properties;

            // Returns a map containing the tiles in that layer that match the TilesetId and the TileIndex
            //std::map<Vector2i, TiledTileData > GetTilesThatMatchTilesetIdAndTileIndex(Int TilesetId, Int TileIndex) const {
            //    std::map<Vector2i, TiledTileData > Result;
            //    for (auto const &TilePair : Tiles) {
            //        if ((TilePair.second.TilesetId == TilesetId) && (TilePair.second.TileIndex == TileIndex)) {
            //            Result.insert(TilePair);
            //        }
            //    }
            //    return Result;
            //}

            //// Returns a map containing the tiles in that layer that match all the TilesetId and the TileIndex of the array TilesetIdAndIndexArray
            //std::map<Vector2i, TiledTileData > GetTilesThatMatchTilesetIdsAndTileIndexs(const std::vector<std::pair<Int, Int>>& TilesetIdAndIndexArray) const {
            //    std::map<Vector2i, TiledTileData > Result;
            //    for (auto const &Pair : TilesetIdAndIndexArray) {
            //        auto tilesThatMatch = GetTilesThatMatchTilesetIdAndTileIndex(Pair.first, Pair.second);
            //        Result.insert(tilesThatMatch.begin(), tilesThatMatch.end());
            //        
            //    }
            //    return Result;
            //}

        };


        // Type enumeration for tilemap orientation
        enum class EAPTiledTileMapOrientation {
            Orthogonal = 0,
            Isometric,
            IsometricStaggered,
            Hexagonal
        };

        // Type enumeration for tilemap render order
        enum class EAPTiledRenderOrder {
            RightDown = 0,
            RightUp,
            LeftDown,
            LeftUp
        };

        // Type enumeration for tilemap staggerind index
        enum class EAPTiledStaggeringIndex {
            Odd = 0,
            Even
        };

        // Type enumeration for tilemap staggering axis
        enum class EAPTiledStaggeringAxis {
            X = 0,
            Y
        };

        /**
        * Holds the parsed data from a tiled tilemap file.
        * Only supports Orthogonal maps for now.
        */
        struct MAGNUM_TILEDIMPORTER_EXPORT TiledTilemapData {
            // Tilemap orientation
            EAPTiledTileMapOrientation Orientation;

            // Width in tiles of the tilemap
            UnsignedInt Width;

            // Height in tiles of the tilemap
            UnsignedInt Height;

            // Tile width in pixels
            UnsignedInt TileWidth;

            // Tile height in pixels
            UnsignedInt TileHeight;

            // Layers of the tilemap (ordered by nearest to farthest) (foreground to background)
            std::vector<TiledLayerData> Layers;

            // Tilesets associated with this tilemap
            std::vector<TiledTilesetData> Tilesets;

            // User specified Key-Value pairs
            TiledPropertiesMapData Properties;

            // Tilemap render order
            EAPTiledRenderOrder RenderOrder = EAPTiledRenderOrder::RightDown;

            // Staggered Axis (Only valid when Orientation is IsometricStaggered or Hexagonal)
            EAPTiledStaggeringAxis StaggeringAxis = EAPTiledStaggeringAxis::Y;

            // Staggered Index (Only valid when Orientation is IsometricStaggered or Hexagonal)
            EAPTiledStaggeringIndex StaggeringIndex = EAPTiledStaggeringIndex::Odd;

            // Hexagonal Tile sides length (Only valid when Orientation is Hexagonal)
            UnsignedInt HexSideLength = 0;

        public:
            // Returns the Tileset Tile Data by the given Tileset Index and Tile index (in that tileset)
            Containers::Optional<const TiledTilesetTileData *> GetTilesetTileData(Int TileIndex, Int TilesetIndex) const {
                if (static_cast<std::size_t>(TilesetIndex) < Tilesets.size()) {
                    return Tilesets[TilesetIndex].GetTileDataByTileIndex(TileIndex);
                }
                return nullptr;
            }

            // Returns the Tileset Tile Data by the given TiledTileData
            Containers::Optional<const TiledTilesetTileData *> GetTilesetTileData(const TiledTileData& TileData) const {
                return GetTilesetTileData(TileData.TileIndex, TileData.TilesetId);
            }
        };

    }
}
