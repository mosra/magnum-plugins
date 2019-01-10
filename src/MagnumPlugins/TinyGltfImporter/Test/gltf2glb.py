#!/usr/bin/env python

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
#             Vladimír Vondruš <mosra@centrum.cz>
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

CHUNK_HEADER_SIZE = 8
GLB_HEADER_SIZE = 12

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input')
    parser.add_argument('--no-embed', action='store_true')
    args = parser.parse_args()

    fileIn = args.input
    fileOut = os.path.splitext(fileIn)[0] + '.glb'

    print("Converting to", fileOut)

    with open(fileIn) as f:
        data = json.load(f)
        binData = bytearray()

        if not args.no_embed and "buffers" in data:
            assert(len(data["buffers"]) <= 1)
            for buffer in data["buffers"]:
                uri = buffer['uri']
                if uri[:5] == 'data:':
                    d = base64.b64decode(uri.split("base64,")[1])
                else:
                    with open(uri, "rb") as bf:
                        d = bf.read()
                binData.extend(d)
                binData.extend(b' '*(4-(len(d)%4)))

        if binData:
            data["buffers"] = [{"byteLength": len(binData)}]

        jsonData = json.dumps(data, separators=(',', ':')).encode()
        # Append padding bytes so that BIN chunk is aligned to 4 bytes
        jsonChunkAlign = (4 - (len(jsonData) % 4)) % 4
        jsonChunkLength = len(jsonData) + jsonChunkAlign

        with open(fileOut, "wb") as outfile:
            gtlf_version = 2
            length = GLB_HEADER_SIZE + CHUNK_HEADER_SIZE + jsonChunkLength
            if binData:
                length += CHUNK_HEADER_SIZE + len(binData)

            # Write header
            outfile.write(b'glTF')
            outfile.write(struct.pack("<II", gtlf_version, length))

            # Write JSON chunk
            outfile.write(struct.pack("<II", jsonChunkLength, CHUNK_TYPE_JSON))
            outfile.write(jsonData)
            outfile.write(b' '*jsonChunkAlign)  # padding

            # Write BIN chunk
            if binData:
                outfile.write(struct.pack("<II", len(binData), CHUNK_TYPE_BIN))
                outfile.write(binData)

if __name__ == "__main__":
    main()
