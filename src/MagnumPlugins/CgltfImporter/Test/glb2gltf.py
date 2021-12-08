#!/usr/bin/env python3

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

CHUNK_TYPE_JSON = 0x4E4F534A
CHUNK_TYPE_BIN = 0x004E4942

glb_header = struct.Struct('<4sII')
chunk_header = struct.Struct('<II')

parser = argparse.ArgumentParser()
parser.add_argument('input')
parser.add_argument('--extract-images', action='store_true')
parser.add_argument('-o', '--output')
args = parser.parse_args()

file_in = args.input
file_out = args.output if args.output else os.path.splitext(file_in)[0] + '.gltf'
file_bin = os.path.splitext(file_out)[0] + '.bin'

def pad_size_32b(size): return (4 - size%4)%4

print("Converting to {} and {}".format(file_out, file_bin))

with open(file_in, 'rb') as f:
    data: bytes = f.read()

# Get header
header, version, length = glb_header.unpack_from(data)
assert header == b'glTF', "glTF signature invalid: %s" % data[:4]
assert version == 2, "glTF version invalid: %d" % version

# Go through chunks. Assume JSON chunk first, BIN chunk next
data = data[glb_header.size:]
chunk_length, chunk_type = chunk_header.unpack_from(data)
assert chunk_type == CHUNK_TYPE_JSON

data = data[chunk_header.size:]
json_data = json.loads(data[:chunk_length].decode('utf-8'))

data = data[chunk_length:]
chunk_length, chunk_type = chunk_header.unpack_from(data)
assert chunk_type == CHUNK_TYPE_BIN
bin_data = data[chunk_header.size:]

assert len(json_data['buffers']) == 1
json_data['buffers'][0]['uri'] = file_bin

# Separate images. To make this easy, we expect the images to be stored in
# a continuous suffix of buffer views.
if args.extract_images:
    image_buffer_views = set()
    earliest_image_buffer_offset = json_data['buffers'][0]['byteLength']
    for image in json_data['images']:
        assert 'bufferView' in image
        assert 'uri' not in image

        # Figure out an extension
        ext = None
        if image['mimeType'] == 'image/jpeg': ext = '.jpg'
        elif image['mimeType'] == 'image/png': ext = '.png'
        elif image['mimeType'] == 'image/x-basis': ext = '.basis'
        elif image['mimeType'] == 'image/unknown':
            if 'name' in image: ext = os.path.splitext(image['name'])[1]
        assert ext, "Unknown MIME type %s" % image['mimeType']

        # Err, if an extension is already present in the name, strip it
        if 'name' in image and image['name'].endswith(ext): ext = ''

        # Remember the earliest buffer view used
        buffer_view_id = image['bufferView']
        image_buffer_views.add(buffer_view_id)

        # Expect there's just one buffer, containing everything. Remember the
        # earliest offset in that buffer
        buffer_view = json_data['bufferViews'][buffer_view_id]
        assert buffer_view['buffer'] == 0
        earliest_image_buffer_offset = min(buffer_view['byteOffset'], earliest_image_buffer_offset)

        # Save the image data. If the image doesn't have a name, pick the glb
        # filename (and assume there's just one image)
        if 'name' not in image:
            assert len(json_data['images']) == 1
            image_out = os.path.splitext(os.path.basename(args.input))[0] + ext
        else:
            image_out = image['name'] + ext
        print("Extracting", image_out)
        with open(image_out, 'wb') as imf:
            imf.write(bin_data[buffer_view['byteOffset']:buffer_view['byteOffset'] + buffer_view['byteLength']])

        # Replace the buffer view reference with a file URI
        del image['bufferView']
        image['uri'] = image_out

    # Check that all buffer views before the first image one are before the
    # first offset as well. I doubt the views will overlap
    for i in range(list(image_buffer_views)[0]):
        assert json_data['bufferViews'][i]['byteOffset'] + json_data['bufferViews'][i]['byteLength'] <= earliest_image_buffer_offset

    # Remove image-related buffer views, move back everything after
    original_offset = 0
    current_id = 0
    original_bin_data = bin_data
    bin_data = bytearray()
    for i, view in enumerate(json_data['bufferViews']):
        # A view that we keep, put it to the new bin data
        if i not in image_buffer_views:
            # TODO: This will probably mess up with alignment when there are
            #   data that are not multiples of 4 bytes
            original_offset = view['byteOffset']
            view['byteOffset'] = len(bin_data)
            bin_data += original_bin_data[original_offset:original_offset + view['byteLength']]

            current_id += 1

        # A view that we remove, update subsequent IDs in all accessors
        else:
            for a in json_data['accessors']:
                if a['bufferView'] > current_id: a['bufferView'] -= 1

    # Remove now unused views
    json_data['bufferViews'] = [view for i, view in enumerate(json_data['bufferViews']) if not i in image_buffer_views]

    # Update with new bin data size
    json_data['buffers'][0]['byteLength'] = len(bin_data)

with open(file_out, 'wb') as of:
    of.write(json.dumps(json_data, indent=2).encode('utf-8'))

with open(file_bin, 'wb') as bf:
    bf.write(bin_data)
