#!/usr/bin/env python3

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
#             Vladimír Vondruš <mosra@centrum.cz>
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

with open(file_out, 'wb') as of:
    of.write(json.dumps(json_data, indent=2).encode('utf-8'))

with open(file_bin, 'wb') as bf:
    bf.write(bin_data)
