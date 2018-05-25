#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2018 Jonathan hale <squareys@googlemail.com>
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

import sys
import os
import json
import struct
import time
import base64


CHUNK_TYPE_JSON = 0x4E4F534A
CHUNK_TYPE_BIN = 0x004E4942

CHUNK_HEADER_SIZE = 8
GLB_HEADER_SIZE = 12

def main():
    fileIn = sys.argv[1]

    print("Converting", fileIn);

    start_time = time.time()

    with open(fileIn) as f:
        data = json.load(f)
        binData = bytearray()

        if "buffers" in data:
            numBuffers = len(data["buffers"])
            if numBuffers > 1:
                print("WARNING: Found too many buffers:", str(numBuffers))

            for buffer in data["buffers"]:
                uri = buffer['uri']
                if uri[-4:] == ".bin":
                    with open(uri, "rb") as bf:
                        d = bf.read()
                elif "base64" in uri: # FIXME: Bad way to detect base64 URL!
                    d = base64.b64decode(uri.split("base64,")[1])
                else:
                    continue
                binData.extend(d)
                binData.extend(b' '*(4-(len(d)%4)))

        binChunkLength = len(binData)
        hasBin = binChunkLength != 0

        if hasBin:
            data["buffers"] = [{"byteLength": binChunkLength}]

        jsonData = json.dumps(data, separators=(',', ':')).encode()
        # Append padding bytes so that BIN chunk is aligned to 4 bytes
        jsonChunkAlign = (4 - (len(jsonData) % 4)) % 4
        jsonChunkLength = len(jsonData) + jsonChunkAlign

        with open(os.path.splitext(fileIn)[0] + ".glb", "wb") as outfile:
            gtlf_version = 2
            length = GLB_HEADER_SIZE + CHUNK_HEADER_SIZE + jsonChunkLength
            if hasBin:
                length += CHUNK_HEADER_SIZE + binChunkLength

            # Write header
            outfile.write(b'glTF')
            outfile.write(struct.pack("<II", gtlf_version, length))

            # Write JSON chunk
            outfile.write(struct.pack("<II", jsonChunkLength, CHUNK_TYPE_JSON))
            outfile.write(jsonData)
            outfile.write(b' '*jsonChunkAlign)  # padding

            # Write BIN chunk
            if hasBin:
                outfile.write(struct.pack("<II", binChunkLength, CHUNK_TYPE_BIN))
                outfile.write(binData)

    print("Done in", str(1000*(time.time() - start_time)), "ms")

if __name__ == "__main__":
    main()
