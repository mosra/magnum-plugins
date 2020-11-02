#ifndef basics_glsl
#define basics_glsl

/* Include in the same directory */
#include "definitions.glsl"
/* Include in parent directory */
#include "../relative.glsl"

/* Including self, should not recurse infinitely due to include guards. */
#include "basics.glsl"
/* Including self again. This checks that we aren't accidentally getting a
   stale view after freeing the file when exiting from the above,
   double-freeing etc. */
#include "basics.glsl"

/* Including a previously included file again. This checks that we aren't
   accidentally getting a stale view after freeing the file when exiting from
   the above, or getting an empty/wrong file after it was closed. */
#ifndef FLOAT_ZERO
#error FLOAT_ZERO not defined after transitively including relative.glsl
#endif
#undef FLOAT_ZERO
#include "../relative.glsl"
#ifndef FLOAT_ZERO
#error FLOAT_ZERO not defined after including relative.glsl again
#endif

#ifdef DEFINITIONS_INCLUDED
vec4 fullScreenTriangle() {
    return vec4(FLOAT_ZERO); /* actually not a triangle, heh */
}
#endif

#endif
