#include <Corrade/Utility/Assert.h>

#include <vector>
#include <string>

/* jpgd, present in all supported versions */

namespace jpgd {

unsigned char* decompress_jpeg_image_from_file(const char*, int*, int*, int*, int, uint32_t) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}

/* apg_bmp, removed in 1.16 */

extern "C" {

unsigned char* apg_bmp_read(const char*, int*, int*, unsigned int*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

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

State::State() {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

State::~State() {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned decode(std::vector<unsigned char>&, unsigned&, unsigned&, const unsigned char*, size_t, LodePNGColorType, unsigned) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned encode(std::vector<unsigned char>&, const unsigned char*, unsigned, unsigned, LodePNGColorType, unsigned) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned load_file(std::vector<unsigned char>&, const std::string&) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

unsigned save_file(const std::vector<unsigned char>&, const std::string&) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}

unsigned lodepng_inspect(unsigned*, unsigned*, LodePNGState*, const unsigned char*, size_t) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

/* pvpng, added in 1.16 */

namespace pv_png {

void* load_png(const void*, size_t, uint32_t, uint32_t&, uint32_t&, uint32_t&) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}

/* tinyexr, added in 1.50 */

extern "C" {

int LoadEXRWithLayer(float **, int*, int*, const char*, const char*, const char**, int*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

typedef struct TEXRHeader EXRHeader;
typedef struct TEXRImage EXRImage;

void InitEXRHeader(EXRHeader*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

void InitEXRImage(EXRImage*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

void FreeEXRErrorMessage(const char*) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

int SaveEXRImageToFile(const EXRImage*, const EXRHeader*, const char*, const char**) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

int LoadEXRFromMemory(float**, int*, int*, const unsigned char*, size_t, const char**) {
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

}
