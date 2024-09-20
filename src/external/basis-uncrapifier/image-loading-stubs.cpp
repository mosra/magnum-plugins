/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2024 Pablo Escobar <mail@rvrs.in>

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

#include <cstdint>
#include <vector>
#include <string>
#include <Corrade/Utility/Assert.h>

/* See README.md for more info. All functions additionally have a declaration
   to avoid -Wmissing-declaration warnings on GCC. */

/* jpgd, present in all supported versions */

namespace jpgd {

unsigned char* decompress_jpeg_image_from_file(const char*, int*, int*, int*, int, uint32_t);
unsigned char* decompress_jpeg_image_from_file(const char*, int*, int*, int*, int, uint32_t) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}

/* apg_bmp, removed in 1.16 */

extern "C" {

unsigned char* apg_bmp_read(const char*, int*, int*, unsigned int*);
unsigned char* apg_bmp_read(const char*, int*, int*, unsigned int*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

void apg_bmp_free(unsigned char*);
void apg_bmp_free(unsigned char*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}

/* lodepng, removed in 1.16 */

/* Can't forward-declare since it has no explicit underlying type */
typedef enum LodePNGColorType {
  LCT_GREY = 0,
  LCT_RGB = 2,
  LCT_PALETTE = 3,
  LCT_GREY_ALPHA = 4,
  LCT_RGBA = 6
} LodePNGColorType;

typedef struct LodePNGState LodePNGState;

namespace lodepng {

class State {
    public:
        State();
        virtual ~State();
};
/* These have to be non-inline otherwise MSVC doesn't compile them at all */
State::State() {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}
State::~State() {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned decode(std::vector<unsigned char>&, unsigned&, unsigned&, const unsigned char*, size_t, LodePNGColorType, unsigned);
unsigned decode(std::vector<unsigned char>&, unsigned&, unsigned&, const unsigned char*, size_t, LodePNGColorType, unsigned) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned encode(std::vector<unsigned char>&, const unsigned char*, unsigned, unsigned, LodePNGColorType, unsigned);
unsigned encode(std::vector<unsigned char>&, const unsigned char*, unsigned, unsigned, LodePNGColorType, unsigned) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned load_file(std::vector<unsigned char>&, const std::string&);
unsigned load_file(std::vector<unsigned char>&, const std::string&) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned save_file(const std::vector<unsigned char>&, const std::string&);
unsigned save_file(const std::vector<unsigned char>&, const std::string&) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}

unsigned lodepng_inspect(unsigned*, unsigned*, LodePNGState*, const unsigned char*, size_t);
unsigned lodepng_inspect(unsigned*, unsigned*, LodePNGState*, const unsigned char*, size_t) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

/* pvpng, added in 1.16 */

namespace pv_png {

void* load_png(const void*, size_t, uint32_t, uint32_t&, uint32_t&, uint32_t&);
void* load_png(const void*, size_t, uint32_t, uint32_t&, uint32_t&, uint32_t&) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}

/* tinyexr, added in 1.50 */

extern "C" {

int LoadEXRWithLayer(float **, int*, int*, const char*, const char*, const char**, int*);
int LoadEXRWithLayer(float **, int*, int*, const char*, const char*, const char**, int*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

typedef struct TEXRHeader EXRHeader;
typedef struct TEXRImage EXRImage;

void InitEXRHeader(EXRHeader*);
void InitEXRHeader(EXRHeader*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

void InitEXRImage(EXRImage*);
void InitEXRImage(EXRImage*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

void FreeEXRErrorMessage(const char*);
void FreeEXRErrorMessage(const char*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

int SaveEXRImageToFile(const EXRImage*, const EXRHeader*, const char*, const char**);
int SaveEXRImageToFile(const EXRImage*, const EXRHeader*, const char*, const char**) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

int LoadEXRFromMemory(float**, int*, int*, const unsigned char*, size_t, const char**);
int LoadEXRFromMemory(float**, int*, int*, const unsigned char*, size_t, const char**) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}
