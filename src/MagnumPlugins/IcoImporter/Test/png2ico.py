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

# Because imagemagick is stupid and can't simply put PNGs into ICOs, I have to
# write my own thing again. SIGH. Usage:
#
#   ./png2ico.py fileWxH.png fileWxH2.png ... file.ico
#
# For simplicity, width and height is parsed from the filenames.

import re
import struct
import sys

filesIn = sys.argv[1:-1]
fileOut = sys.argv[-1]

print("Converting to", fileOut)

filePattern = re.compile(r'.+(\d+)x(\d+).png')

with open(fileOut, 'wb') as output:
    output.write(struct.pack('<HHH', 0, 1, len(filesIn)))
    offset = 6 + 16*len(filesIn)

    data = bytearray()
    for file in filesIn:
        with open(file, 'rb') as input:
            contents = input.read()

        size = filePattern.match(file)
        output.write(struct.pack('<BBBB HH II',
            int(size[1]) % 256,
            int(size[2]) % 256,
            0, 0, 0, 0,
            len(contents), 6 + 16*len(filesIn) + len(data)))

        data += contents

    output.write(data)
