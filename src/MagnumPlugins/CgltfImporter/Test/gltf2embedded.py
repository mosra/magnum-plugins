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

import base64
import json
import os
import sys
import struct

fileIn = sys.argv[1]
fileOut = os.path.splitext(fileIn)[0] + '-embedded.gltf'

print("Converting to", fileOut)

with open(fileIn) as f:
    data = json.load(f)
    if 'buffers' in data:
        for buffer in data['buffers']:
            uri = buffer['uri']
            if uri[:5] != 'data:':
                with open(uri, "rb") as bf:
                    buffer['uri'] = 'data:application/octet-stream;base64,' + base64.b64encode(bf.read()).decode('utf-8')

    if 'images' in data:
        for image in data['images']:
            if 'uri' not in image: continue
            uri = image['uri']
            if uri[:5] != 'data:':
                if uri.endswith('.png'):
                    mime = 'image/png'
                elif uri.endswith('.basis'):
                    # https://github.com/BabylonJS/Babylon.js/issues/6636 and
                    # https://github.com/BinomialLLC/basis_universal/issues/52.
                    # Can't use image/x-basis because TinyGLTF has a whitelist
                    # of MIME types, FFS.
                    # https://github.com/syoyo/tinygltf/blob/7e009041e35b999fd1e47c0f0e42cadcf8f5c31c/tiny_gltf.h#L2706
                    mime = 'application/octet-stream'
                else:
                    assert False, ("unsupported file type %s" % uri)
                with open(uri, "rb") as bf:
                    image['uri'] = 'data:{};base64,{}'.format(mime, base64.b64encode(bf.read()).decode('utf-8'))

with open(fileOut, 'wb') as output:
    output.write(json.dumps(data, separators=(',', ':')).encode())
