#ifndef Magnum_Trade_decode_h
#define Magnum_Trade_decode_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Macros.h>
#include <Magnum/Magnum.h>

namespace Magnum { namespace Trade { namespace {

/* Used only by GltfImporter, but put into a dedicated header for easier
   testing */

/* Decode percent-encoded characters in URIs:
   https://datatracker.ietf.org/doc/html/rfc3986#section-2.1 */
Containers::Optional<Containers::String> decodeUri(const char* const errorPrefix, Containers::StringView uri) {
    const std::size_t size = uri.size();
    Containers::String out{NoInit, size};
    std::size_t iOut = 0;
    for(std::size_t i = 0; i != size; ++i) {
        char c = uri[i];
        /** @todo profile and speed up with find('%') */
        if(c == '%') {
            if(i + 2 >= size) {
                Error{} << errorPrefix << "invalid URI escape sequence" << uri.slice(i, size);
                return {};
            }

            c = 0;
            for(const char j: {uri[i + 1], uri[i + 2]}) {
                c <<= 4;
                if(j >= '0' && j <= '9') c |= j - '0';
                else if(j >= 'A' && j <= 'F') c |= 10 + j - 'A';
                else if(j >= 'a' && j <= 'f') c |= 10 + j - 'a';
                else {
                    Error{} << errorPrefix << "invalid URI escape sequence" << uri.slice(i, i + 3);
                    return {};
                }
            }

            i += 2;
        }

        out[iOut++] = c;
    }

    out[iOut] = '\0';

    if(out.isSmall())
        return Containers::String{out.data(), iOut};
    else
        return Containers::String{out.release(), iOut, nullptr};
}

/* The -1s get expanded to 0xffffffff when cast to an UnsignedInt and then we
   check each three bytes of the output against 0xff000000 to catch invalid
   input characters */
constexpr Byte Base64Values[]{
    /*0x00*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0x10*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
           /*                                             +               / */
    /*0x20*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
           /* 0   1   2   3   4   5   6   7   8   9                         */
    /*0x30*/ 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
          /*      A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    /*0x40*/ -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
          /*  P   Q   R   S   T   U   V   W   X   Y   Z                     */
    /*0x50*/ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
          /*      a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    /*0x60*/ -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
          /*  p   q   r   s   t   u   v   w   x   y   z                     */
    /*0x70*/ 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    /*0x80*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0x90*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0xa0*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0xb0*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0xc0*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0xd0*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0xe0*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /*0xf0*/ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/* Loosely based off https://stackoverflow.com/a/37109258, with ... basically
   just the table left and the rest reworked from scratch to properly report
   errors, not miscalculate output size, not do OOB reads... Sigh. */
Containers::Optional<Containers::Array<char>> decodeBase64(const char* const errorPrefix, const Containers::StringView string) {
    /* Cast the input to an unsigned type so we don't accidentally index with a
       negative number into the table */
    const UnsignedByte* const in = reinterpret_cast<const UnsignedByte*>(string.data());
    const std::size_t size = string.size();

    /* Calculate output size. If the size is divisble by 4, check for padding
       bytes and decide based on them. */
    std::size_t sizeFullBlocks, pad1, pad2;
    if(size && (size & 0x3) == 0) {
        /* The padded block can be only ???= or ??==. Anything else is an error
           that'll fire later when decoding. */
        if(in[size - 1] == '=') {
            sizeFullBlocks = size - 4;
            pad1 = 1;
            pad2 = in[size - 2] != '=';
        } else {
            sizeFullBlocks = size;
            pad1 = pad2 = 0;
        }

    /* Otherwise assume the output size is directly determined by the size of
       the input. If padding =s are indeed present in this case, it's an error
       that'll fire later when decoding. */
    } else {
        sizeFullBlocks = size & ~0x3;
        if((size & 0x03) == 1) {
            Error{} << errorPrefix << "invalid Base64 length" << size << Debug::nospace << ", expected 0, 2 or 3 padding bytes but got 1";
            return {};
        }

        pad1 = (size & 0x3) > 1;
        pad2 = (size & 0x3) > 2;
    }

    /* Size of the output is 3/4 of the input full blocks plus one or two
       padding bytes. Cast it as well, don't want to play with fire when
       converting bytes > 127 to a char */
    Containers::Array<char> data{NoInit, sizeFullBlocks*3/4 + pad1 + pad2};
    UnsignedByte* out = reinterpret_cast<UnsignedByte*>(data.data());

    std::size_t iOut = 0;
    for(std::size_t i = 0; i != sizeFullBlocks; i += 4, iOut += 3) {
        const UnsignedInt n =
            UnsignedInt(Base64Values[in[i + 0]]) << 18 |
            UnsignedInt(Base64Values[in[i + 1]]) << 12 |
            UnsignedInt(Base64Values[in[i + 2]]) <<  6 |
            UnsignedInt(Base64Values[in[i + 3]]) <<  0;
        if CORRADE_UNLIKELY(n & 0xff000000u) {
            Error{} << errorPrefix << "invalid Base64 block" << string.slice(i, i + 4);
            return {};
        }

        out[iOut + 0] =  n >> 16;
        out[iOut + 1] = (n >>  8) & 0xff;
        out[iOut + 2] = (n >>  0) & 0xff;
    }

    UnsignedInt n;
    if(pad1) n =
        UnsignedInt(Base64Values[in[sizeFullBlocks + 0]]) << 18 |
        UnsignedInt(Base64Values[in[sizeFullBlocks + 1]]) << 12;
    if(pad2) n |=
        UnsignedInt(Base64Values[in[sizeFullBlocks + 2]]) <<  6;
    if CORRADE_UNLIKELY(pad1 && (n & 0xff000000u)) {
        Error{} << errorPrefix << "invalid Base64 padding bytes" << string.slice(sizeFullBlocks, sizeFullBlocks + 1 + pad1 + pad2);
        return {};
    }

    if(pad1)
        out[iOut + 0] =  n >> 16;
    if(pad2)
        out[iOut + 1] = (n >>  8) & 0xff;

    /* GCC 4.8 and Clang 3.8 need extra help here */
    return Containers::optional(std::move(data));
}

}}}

#endif
