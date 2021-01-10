#!/usr/bin/env python3

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2018 Jonathan Hale <squareys@googlemail.com>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

import argparse
import os
import json
import struct
import base64

CHUNK_TYPE_JSON = 0x4E4F534A
CHUNK_TYPE_BIN = 0x004E4942

glb_header = struct.Struct('<4sII')
chunk_header = struct.Struct('<II')

def pad_size_32b(size): return (4 - size%4)%4

parser = argparse.ArgumentParser()
parser.add_argument('input')
parser.add_argument('--no-embed', action='store_true')
parser.add_argument('--bundle-images', action='store_true')
parser.add_argument('-o', '--output')
args = parser.parse_args()

file_in = args.input
file_out = args.output if args.output else os.path.splitext(file_in)[0] + '.glb'

print("Converting to", file_out)

with open(file_in) as f:
    data = json.load(f)

bin_data = bytearray()

if not args.no_embed and "buffers" in data:
    assert len(data['buffers']) <= 1
    if data['buffers']:
        uri = data['buffers'][0]['uri']
        if uri[:5] == 'data:':
            d = base64.b64decode(uri.split('base64,')[1])
        else:
            with open(uri, 'rb') as bf:
                d = bf.read()
        bin_data.extend(d)

if args.bundle_images:
    assert "buffers" in data
    assert not args.no_embed

    for image in data['images']:
        assert 'bufferView' not in image
        assert 'uri' in image

        # Set image name and mime type if not already present, so we have
        # something to extract the file name / extension from in glb2gltf
        basename, ext = os.path.splitext(image['uri'])
        if 'mimeType' not in image:
            if ext == '.jpg': image['mimeType'] = 'image/jpeg'
            elif ext == '.png': image['mimeType'] = 'image/png'
            elif ext == '.basis': image['mimeType'] = 'image/x-basis'
            else: assert False, "Unknown image file extension %s" % ext
        if 'name' not in image: image['name'] = basename

        # Load the image data, put it into a new buffer view
        print("Bundling", image['uri'])
        with open(image['uri'], 'rb') as imf:
            image_data = imf.read()
        data['bufferViews'] += [{
            'buffer': 0,
            'byteOffset': len(bin_data),
            'byteLength': len(image_data)
        }]
        bin_data += image_data
        del image['uri']
        image['bufferView'] = len(data['bufferViews']) - 1

# Pad the buffer, update its length
if not args.no_embed and 'buffers' in data and data['buffers']:
    bin_data.extend(b' '*pad_size_32b(len(bin_data)))

    del data['buffers'][0]['uri']
    data['buffers'][0]['byteLength'] = len(bin_data)

json_data = json.dumps(data, separators=(',', ':')).encode('utf-8')
# Append padding bytes so that BIN chunk is aligned to 4 bytes
json_chunk_align = pad_size_32b(len(json_data))
json_chunk_length = len(json_data) + json_chunk_align

with open(file_out, 'wb') as outfile:
    length = glb_header.size + chunk_header.size + json_chunk_length
    if bin_data:
        length += chunk_header.size + len(bin_data)

    # Write header
    outfile.write(glb_header.pack(b'glTF', 2, length))

    # Write JSON chunk
    outfile.write(chunk_header.pack(json_chunk_length, CHUNK_TYPE_JSON))
    outfile.write(json_data)
    outfile.write(b' '*json_chunk_align)

    # Write BIN chunk
    if bin_data:
        outfile.write(chunk_header.pack(len(bin_data), CHUNK_TYPE_BIN))
        outfile.write(bin_data)
