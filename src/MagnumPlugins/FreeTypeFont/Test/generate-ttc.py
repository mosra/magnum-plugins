#!/usr/bin/env python3

# Generates Oxygen.ttc from Oxygen.ttf. The second face is identical except for
# the character map, where 'W' resolves to glyph 72 instead of 58.

from pathlib import Path
import struct

# To reduce test data sizes, the subset file is made out of Oxygen.ttf with
#   hb-subset Oxygen.ttf --glyphs="gid58,gid69,gid89,gid72" -o Oxygen.subset.ttf
# extracting just the W a v e glyphs, respectively
Source = Path(__file__).with_name("Oxygen.subset.ttf")
Destination = Path(__file__).with_name("Oxygen.ttc")

def checksum(data):
    padded = data + b"\0"*((4 - len(data)) % 4)
    return sum(struct.unpack(">%dI" % (len(padded)//4), padded)) & 0xffffffff

def patch_w_glyph(font):
    cmap_offset = table_records(font)[b"cmap"][2]
    _, subtable_count = struct.unpack_from(">HH", font, cmap_offset)

    for i in range(subtable_count):
        subtable_offset = struct.unpack_from(">I", font, cmap_offset + 4 + i*8 + 4)[0]
        table = cmap_offset + subtable_offset
        fmt = struct.unpack_from(">H", font, table)[0]

        if fmt == 4:
            segment_count = struct.unpack_from(">H", font, table + 6)[0]//2
            end_codes = table + 14
            start_codes = end_codes + 2*segment_count + 2
            id_deltas = start_codes + 2*segment_count
            id_range_offsets = id_deltas + 2*segment_count

            for j in range(segment_count):
                end = struct.unpack_from(">H", font, end_codes + 2*j)[0]
                start = struct.unpack_from(">H", font, start_codes + 2*j)[0]
                range_offset = struct.unpack_from(">H", font, id_range_offsets + 2*j)[0]
                if start <= ord("W") <= end and range_offset == 0:
                    struct.pack_into(">h", font, id_deltas + 2*j, -84)

        elif fmt == 6:
            first_code, entry_count = struct.unpack_from(">HH", font, table + 6)
            if first_code <= ord("W") < first_code + entry_count:
                struct.pack_into(">H", font, table + 10 + 2*(ord("W") - first_code), 3)

def table_records(font, base=0):
    table_count = struct.unpack_from(">H", font, base + 4)[0]
    records = {}
    for i in range(table_count):
        record = base + 12 + i*16
        tag = bytes(font[record:record + 4])
        records[tag] = (record, *struct.unpack_from(">III", font, record + 4))
    return records

def copy_font(output, font, base):
    output[base:base + len(font)] = font
    for record, _, offset, _ in table_records(output, base).values():
        struct.pack_into(">I", output, record + 8, base + offset)

def update_font_checksums(output, base, length):
    records = table_records(output, base)
    head_offset = records[b"head"][2]

    struct.pack_into(">I", output, head_offset + 8, 0)
    for record, _, offset, size in records.values():
        struct.pack_into(">I", output, record + 4, checksum(output[offset:offset + size]))

    adjustment = (0xb1b0afba - checksum(output[base:base + length])) & 0xffffffff
    struct.pack_into(">I", output, head_offset + 8, adjustment)

def aligned4(size):
    return (size + 3) & ~3

original = bytearray(Source.read_bytes())
modified = bytearray(original)
patch_w_glyph(modified)

header_size = 12 + 2*4
font_offsets = [header_size, header_size + aligned4(len(original))]
output = bytearray(font_offsets[1] + len(modified))

output[:12] = b"ttcf" + struct.pack(">II", 0x00010000, 2)
struct.pack_into(">II", output, 12, font_offsets[0], font_offsets[1])

copy_font(output, original, font_offsets[0])
copy_font(output, modified, font_offsets[1])
update_font_checksums(output, font_offsets[0], len(original))
update_font_checksums(output, font_offsets[1], len(modified))

Destination.write_bytes(output)
