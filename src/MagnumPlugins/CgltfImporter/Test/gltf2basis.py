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

parser = argparse.ArgumentParser()
parser.add_argument('input')
parser.add_argument('output')
args = parser.parse_args()

print("Converting to", args.output)

with open(args.input) as f:
    json_data = json.load(f)

# Replace all images URIs with files that have a .basis extension (and expect
# they exist)
for image in json_data['images']:
    assert 'bufferView' not in image
    assert 'uri' in image

    image['uri'] = os.path.splitext(image['uri'])[0] + '.basis'
    assert os.path.exists(image['uri']), "Image does not exist: %s" % image['uri']
    image['mimeType'] = 'image/x-basis'

# For all textures, replace an image source reference with the totally
# unnecessary GOOGLE_texture_basis extension (why, gltf, WHY)
for texture in json_data['textures']:
    texture['extensions'] = {
        'GOOGLE_texture_basis': {
            'source': texture['source']
        }
    }
    del texture['source']

with open(args.output, 'wb') as of:
    of.write(json.dumps(json_data, indent=2).encode('utf-8'))
