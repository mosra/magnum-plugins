#!/usr/bin/env python3

# Generates MonochromeBitmap.ttf and Gray4Bitmap.ttf from Oxygen.ttf. Both are
# subsets containing only the X glyph with its vector outline stripped and a
# 16x16 embedded bitmap strike added in EBDT/EBLC. The first uses 1bpp bitmap
# data, the second uses 4bpp data.

from pathlib import Path
import subprocess
import tempfile

from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.tables import E_B_D_T_, E_B_L_C_
from fontTools.pens.ttGlyphPen import TTGlyphPen
from PIL import Image


Directory = Path(__file__).parent
Source = Directory/"Oxygen.ttf"

MonoRows = [
    "#..............#",
    ".#............#.",
    "..#..........#..",
    "...#........#...",
    "....#......#....",
    ".....#....#.....",
    "......#..#......",
    ".......##.......",
    ".......##.......",
    "......#..#......",
    ".....#....#.....",
    "....#......#....",
    "...#........#...",
    "..#..........#..",
    ".#............#.",
    "#..............#",
]

Gray4Rows = [
    "1..............f",
    ".2............e.",
    "..3..........d..",
    "...4........c...",
    "....5......b....",
    ".....6....a.....",
    "......7..9......",
    ".......88.......",
    ".......88.......",
    "......7..9......",
    ".....6....a.....",
    "....5......b....",
    "...4........c...",
    "..3..........d..",
    ".2............e.",
    "1..............f",
]


def subset_oxygen(destination):
    subprocess.run([
        "pyftsubset",
        str(Source),
        f"--output-file={destination}",
        "--unicodes=U+0058",
        "--notdef-glyph",
        "--notdef-outline",
        "--recommended-glyphs",
        "--layout-features=*",
        "--name-IDs=*",
        "--name-legacy",
        "--name-languages=*"
    ], check=True)


def small_metrics():
    metrics = E_B_D_T_.SmallGlyphMetrics()
    metrics.height = 16
    metrics.width = 16
    metrics.BearingX = 0
    metrics.BearingY = 16
    metrics.Advance = 16
    return metrics


def line_metrics():
    metrics = E_B_L_C_.SbitLineMetrics()
    metrics.ascender = 16
    metrics.descender = 0
    metrics.widthMax = 16
    metrics.caretSlopeNumerator = 1
    metrics.caretSlopeDenominator = 0
    metrics.caretOffset = 0
    metrics.minOriginSB = 0
    metrics.minAdvanceSB = 0
    metrics.maxBeforeBL = 16
    metrics.minAfterBL = 0
    metrics.pad1 = 0
    metrics.pad2 = 0
    return metrics


def encode_mono(rows):
    data = bytearray()
    for row in rows:
        value = 0
        for x, pixel in enumerate(row):
            if pixel == "#":
                value |= 0x8000 >> x
        data += value.to_bytes(2, "big")
    return bytes(data)


def encode_gray4(rows):
    data = bytearray()
    for row in rows:
        for x in range(0, 16, 2):
            def nibble(pixel):
                return 0 if pixel == "." else int(pixel, 16)
            data.append((nibble(row[x]) << 4) | nibble(row[x + 1]))
    return bytes(data)


def pixels_mono(rows):
    return [[255 if pixel == "#" else 0 for pixel in row] for row in rows]


def pixels_gray4(rows):
    def value(pixel):
        return 0 if pixel == "." else int(pixel, 16)*17
    return [[value(pixel) for pixel in row] for row in rows]


def save_png(path, pixels):
    image = Image.new("L", (len(pixels[0]), len(pixels)))
    image.putdata([pixel for row in pixels for pixel in row])
    image.save(path)


def add_bitmap_strike(font, bit_depth, image_data):
    glyph_name = font.getBestCmap()[ord("X")]
    font["glyf"][glyph_name] = TTGlyphPen(None).glyph()

    bitmap = E_B_D_T_.ebdt_bitmap_format_2(b"", font)
    bitmap.metrics = small_metrics()
    bitmap.imageData = image_data

    ebdt = newTable("EBDT")
    ebdt.version = 2.0
    ebdt.strikeData = [{glyph_name: bitmap}]
    font["EBDT"] = ebdt

    eblc = newTable("EBLC")
    eblc.version = 2.0

    strike = E_B_L_C_.Strike()
    strike.bitmapSizeTable.colorRef = 0
    strike.bitmapSizeTable.hori = line_metrics()
    strike.bitmapSizeTable.vert = line_metrics()
    strike.bitmapSizeTable.ppemX = 16
    strike.bitmapSizeTable.ppemY = 16
    strike.bitmapSizeTable.bitDepth = bit_depth
    strike.bitmapSizeTable.flags = 1

    subtable = E_B_L_C_.eblc_index_sub_table_1(b"", font)
    subtable.indexFormat = 1
    subtable.imageFormat = 2
    subtable.imageDataOffset = 0
    subtable.names = [glyph_name]
    subtable.locations = []
    strike.indexSubTables = [subtable]

    eblc.strikes = [strike]
    font["EBLC"] = eblc


def save_fixture(subset, destination, bit_depth, image_data):
    font = TTFont(subset, recalcTimestamp=False)
    font["head"].modified = font["head"].created
    add_bitmap_strike(font, bit_depth, image_data)
    font.save(destination)


with tempfile.TemporaryDirectory() as tmp:
    subset = Path(tmp)/"OxygenX.subset.ttf"
    subset_oxygen(subset)
    save_fixture(subset, Directory/"MonochromeBitmap.ttf", 1, encode_mono(MonoRows))
    save_fixture(subset, Directory/"Gray4Bitmap.ttf", 4, encode_gray4(Gray4Rows))
    save_png(Directory/"glyph-cache-monochrome-bitmap.png", pixels_mono(MonoRows))
    save_png(Directory/"glyph-cache-gray4-bitmap.png", pixels_gray4(Gray4Rows))
